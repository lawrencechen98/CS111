/*
 * NAME: Lawrence Chen
 * EMAIL: lawrencechen98@gmail.com
 * ID: XXXXXXXXX
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <zlib.h>

//for inter-process communication with pipe()
int to_pipefd[2];
int from_pipefd[2];
pid_t child_pid = -1;

int compress_flag = 0;

int sockfd;

z_stream to_client;
z_stream from_client;

//file descriptors for poll()
struct pollfd pollfds[2];

//create just one buffer
const int BUFFER_SIZE = 2048;
char buffer[2048];

//function to handle general errors and print to stderr
void error_handler(char* func, int err) {
	fprintf(stderr, "Error in %s: %s\r\n", func, strerror(err));
	exit(1);
}

//custom close() function that handles error checking (because close was called so many times)
void close_with_error_check(int fd) {
	if (close(fd) == -1) {
		error_handler("close()", errno);
	}
}

//handler for sigpipe signal
void sigpipe_handler() {
	while(1) {
                if (pollfds[1].revents & POLLIN) {
                        int num_read = read (from_pipefd[0], buffer, BUFFER_SIZE);
                        if (num_read < 0) {
                                error_handler("read()", errno);
                        }
                        for (int i = 0; i < num_read; i++) {
                                if (buffer[i] == '\n') {
                                        char newline[2] = "\r\n";
                                        if (write(STDOUT_FILENO, newline, 2) < 0) {
                                                error_handler("write()", errno);
                                        }
                                } else if (buffer[i] == 4) {
                                        exit(0);
                                } else {
                                        if (write(sockfd, buffer + i, 1) < 0) {
                                                error_handler("write()", errno);
                                        }
                                }
                        }
                }

                if (pollfds[0].revents & (POLLERR | POLLHUP) || pollfds[1].revents & (POLLERR | POLLHUP)) {
                        exit(0);
                }
        }
	exit(0);
}

//function to get status before exit()
void get_status() {
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
	
	int status;
    	if (waitpid(child_pid, &status, 0) != -1) {
		fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", status&0x7f, (status&0xff00) >> 8);         	
  	} else {
		error_handler("waitpid()", errno);
	}
}

//function that forks another process to run shell program, as well as set up pipes and polls
void fork_shell() {
	if (pipe(to_pipefd) == -1) {
		error_handler("pipe()", errno);
	}
	if (pipe(from_pipefd) == -1) {
		error_handler("pipe()", errno);
	}

	child_pid = fork();

	if (child_pid > 0) {	
		close_with_error_check(to_pipefd[0]);
		close_with_error_check(from_pipefd[1]);
		
		pollfds[0].fd = sockfd;
		pollfds[0].events = POLLIN;

		pollfds[1].fd = from_pipefd[0];
		pollfds[1].events = POLLIN;

		if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
			error_handler("signal()", errno);
		}
		if (atexit(get_status) == -1) {
			error_handler("atexit()", errno);
		}
		
	} else if (child_pid == 0) {
		close_with_error_check(to_pipefd[1]);
		close_with_error_check(from_pipefd[0]);

		close(sockfd);

		if (dup2(to_pipefd[0], STDIN_FILENO) == -1) {
			error_handler("dup2()", errno);
		}
		close_with_error_check(to_pipefd[0]);
		if (dup2(from_pipefd[1], STDOUT_FILENO) == -1) {
			error_handler("dup2()", errno);
		}
		if (dup2(from_pipefd[1], STDERR_FILENO) == -1) {
                        error_handler("dup2()", errno);
                }
		close_with_error_check(from_pipefd[1]);

		char *args[2] = {"/bin/bash", NULL};
		if (execvp(args[0],args) == -1) {
			error_handler("execvp()", errno);
		}
	} else {
		error_handler("fork()", errno);
	}
}

void cleanup_compression() {
        deflateEnd(&to_client);
        inflateEnd(&from_client);
}

void setup_compression() {
        if (atexit (cleanup_compression) == -1) {
                error_handler("atexit()", errno);
        }
        to_client.zalloc = Z_NULL;
        to_client.zfree = Z_NULL;
        to_client.opaque = Z_NULL;
        if (deflateInit(&to_client, Z_DEFAULT_COMPRESSION) != Z_OK) {
                fprintf(stderr, "deflateInit(): %s\n", to_client.msg);
                exit(1);
        }

        from_client.zalloc = Z_NULL;
        from_client.zfree = Z_NULL;
        from_client.opaque = Z_NULL;
        if (inflateInit(&from_client) != Z_OK) {
                fprintf(stderr, "inflateInit(): %s\n", from_client.msg);
                exit(1);
        }
}

int compression(char* buf, int num_read) {
        int num_bytes;
        char temp_buf[BUFFER_SIZE];
        memcpy(temp_buf, buf, num_read);

        to_client.avail_in = num_read;
        to_client.next_in = (Bytef *) temp_buf;
        to_client.avail_out = BUFFER_SIZE;
        to_client.next_out = (Bytef *) buf;

        do {
                deflate(&to_client, Z_SYNC_FLUSH);
        } while (to_client.avail_in > 0);

        num_bytes = BUFFER_SIZE - to_client.avail_out;
        return num_bytes;
}

int decompression(char* buf, int num_read) {
        int num_bytes;
        char temp_buf[BUFFER_SIZE];
        memcpy(temp_buf, buf, num_read);

        from_client.avail_in = num_read;
        from_client.next_in = (Bytef *) temp_buf;
        from_client.avail_out = BUFFER_SIZE;
        from_client.next_out = (Bytef *) buf;

        do {
                inflate(&from_client, Z_SYNC_FLUSH);
        } while (from_client.avail_in > 0);

        num_bytes = BUFFER_SIZE - from_client.avail_out;
        return num_bytes;
}

//function to run main I/O loop for when --shell flag is indicated
void client_shell_listen() {
	fork_shell();

	while(1) {
		int ret = poll(pollfds, 2, 0);
		if (ret < 0) {
			error_handler("poll()", errno);
		}
		if (ret > 0) {
			if (pollfds[0].revents & POLLIN) {
				int num_read = read (sockfd, buffer, BUFFER_SIZE);
				if (num_read < 0) {
					error_handler("read()", errno);
				}
				if (compress_flag) {
                                        num_read = decompression(buffer, num_read);
                                }
				for (int i = 0; i < num_read; i++) {
					if (buffer[i] == '\r' || buffer[i] == '\n') {
						char newline[1] = "\n";
						if (write(to_pipefd[1], newline, 1) < 0) {
							error_handler("write()", errno);
						}
					} else if (buffer[i] == 4) {
						close(to_pipefd[1]);
					} else if (buffer[i] == 3) {
						kill(child_pid, SIGINT);
					} else {
						if (write(to_pipefd[1], buffer + i, 1) < 0) {
                                                        error_handler("write()", errno);
                                                }
					}
				}
			}

			if (pollfds[1].revents & POLLIN) {
				int num_read = read (from_pipefd[0], buffer, BUFFER_SIZE);
                                if (num_read < 0) {
                                        error_handler("read()", errno);
                                }
                                for (int i = 0; i < num_read; i++) {
                                        if (buffer[i] == 4) {
						exit(0);
                                        }
                                }
				if (compress_flag) {
					num_read = compression(buffer, num_read);
				}
				if (write(sockfd, buffer, num_read) < 0) {
                                        error_handler("write()", errno);
                                }

			}

			if (pollfds[0].revents & (POLLERR | POLLHUP) || pollfds[1].revents & (POLLERR | POLLHUP)) {
				exit(0);
			}
		}
	}
	
}
int socket_handler(int port_no) {
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error_handler("socket()", errno);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port_no);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		error_handler("bind()", errno);
	}

	/* Now start listening for the clients, here process will
	* go in sleep mode and will wait for the incoming connection
	*/

	listen(sockfd,5);
	clilen = sizeof(cli_addr);

	/* Accept actual connection from the client */
	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
	if (newsockfd < 0) {
		error_handler("accept()", errno);
	}

	return newsockfd;
}

//main function
int main(int argc, char** argv) {

        int port_no = -1;

        struct option options[] = {
                {"port", 1, NULL, 'p'},
                {"compress", 0, NULL, 'c'}
        };
        int oc;
        while ((oc = getopt_long(argc, argv, "s", options, NULL)) != -1) {
                switch (oc) {
                        case 'p':
                                port_no = atoi(optarg);
                                break;
                        case 'c':
                                compress_flag = 1;
                                setup_compression();
				break;
                        default:
                                fprintf(stderr, "usage: lab1b-client --port=PORTNUMBER [--compress]\r\n");
                                exit(1);
                }
        }
        if (port_no < 0) {
                fprintf(stderr, "usage: lab1b-server --port=PORTNUMBER [--compress]\r\n");
                exit(1);
        }

        sockfd = socket_handler(port_no);
	
	client_shell_listen();
  	exit(0);
}

