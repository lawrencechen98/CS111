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

//save old terminal attributes
struct termios saved_attributes;

const int BUFFER_SIZE = 2048;
char buffer[2048];

struct pollfd pollfds[2];

const char *HOSTNAME = "localhost";

z_stream to_shell;
z_stream from_shell;

int log_fd = -1;
int compress_flag = 0;

void error_handler(char* func, int err) {
	fprintf(stderr, "Error in %s: %s\r\n", func, strerror(err));
	exit(1);
}

void reset_input_mode() {
       tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}

void set_input_mode() {

	if (tcgetattr (STDIN_FILENO, &saved_attributes) == -1) {
		error_handler("tcgetattr()", errno);
	}
  	if (atexit (reset_input_mode) == -1) {
		error_handler("atexit()", errno);
	}

  	struct termios tattr;
	if (tcgetattr (STDIN_FILENO, &tattr) == -1) {
                error_handler("tcgetattr()", errno);
        }
	tattr.c_iflag = ISTRIP;	/* only lower 7 bits	*/
	tattr.c_oflag = 0;		/* no processing	*/
	tattr.c_lflag = 0;		/* no processing	*/
	if (tcsetattr (STDIN_FILENO, TCSANOW, &tattr) == -1) {
		error_handler("tcsetattr()", errno);
	}
}

int socket_handler(int port_no) {
	
	struct sockaddr_in serv_addr;
	struct hostent* server;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error_handler("socket()", errno);
	}
	
	server = gethostbyname(HOSTNAME);
	if (server == NULL) {
		error_handler("gethostbyname()", errno);
	}
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char *) &serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port_no);

	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		error_handler("connect()", errno);
	}

	return sockfd;
}

void cleanup_compression() {
	deflateEnd(&to_shell);
	inflateEnd(&from_shell);
}

void setup_compression() {
	if (atexit (cleanup_compression) == -1) {
		error_handler("atexit()", errno);
	}
	to_shell.zalloc = Z_NULL;
	to_shell.zfree = Z_NULL;
	to_shell.opaque = Z_NULL;
	if (deflateInit(&to_shell, Z_DEFAULT_COMPRESSION) != Z_OK) {
		fprintf(stderr, "deflateInit(): %s\n", to_shell.msg);
		exit(1);
	}
	
	from_shell.zalloc = Z_NULL;
	from_shell.zfree = Z_NULL;
	from_shell.opaque = Z_NULL;
	if (inflateInit(&from_shell) != Z_OK) {
                fprintf(stderr, "inflateInit(): %s\n", from_shell.msg);
                exit(1);
        }   
}

int compression(char* buf, int num_read) {
	int num_bytes;
	char temp_buf[BUFFER_SIZE];
	memcpy(temp_buf, buf, num_read);

	to_shell.avail_in = num_read;
	to_shell.next_in = (Bytef *) temp_buf;
	to_shell.avail_out = BUFFER_SIZE;
	to_shell.next_out = (Bytef *) buf;

	do {
		deflate(&to_shell, Z_SYNC_FLUSH);
	} while (to_shell.avail_in > 0);

	num_bytes = BUFFER_SIZE - to_shell.avail_out;
	return num_bytes;
}

int decompression(char* buf, int num_read) {
	int num_bytes;
	char temp_buf[BUFFER_SIZE];
        memcpy(temp_buf, buf, num_read);

        from_shell.avail_in = num_read;
        from_shell.next_in = (Bytef *) temp_buf;
        from_shell.avail_out = BUFFER_SIZE;
        from_shell.next_out = (Bytef *) buf;

        do {
                inflate(&from_shell, Z_SYNC_FLUSH);
        } while (from_shell.avail_in > 0);

        num_bytes = BUFFER_SIZE - from_shell.avail_out;
	return num_bytes;
}

void keyboard_socket_listen() {

	while(1) {
		int ret = poll(pollfds, 2, 0);
		if (ret < 0) {
			error_handler("poll()", errno);
		}
		if (ret > 0) {
			if (pollfds[0].revents & POLLIN) {
				int num_read = read (STDIN_FILENO, buffer, BUFFER_SIZE);
				if (num_read < 0) {
					error_handler("read()", errno);
				}
				for (int i = 0; i < num_read; i++) {
					if (buffer[i] == '\r' || buffer[i] == '\n') {
						char newline[2] = "\r\n";
						if (write(STDOUT_FILENO, newline, 2) < 0) {
							error_handler("write()", errno);
						}
					} else {
						if (write(STDOUT_FILENO, buffer + i, 1) < 0) {
							error_handler("write()", errno);
						}
					}
				}
				for (int i = 0; i < num_read; i++) {
					if (buffer[i] == '\r') {
						buffer[i] = '\n';
					}
				}
				if (compress_flag) {
                                        num_read = compression(buffer, num_read);
				}	
				if (write(pollfds[1].fd, buffer, num_read) < 0) {
					error_handler("write()", errno);
				}
				if (log_fd >=  0) {
					if (dprintf(log_fd, "SENT %d bytes: ", num_read) < 0) {
                                                error_handler("dprintf()", errno);
                                        }
                                        if (write(log_fd, buffer, num_read) < 0) {
                                                error_handler("write()", errno);
                                        } 
                                        if (dprintf(log_fd, "\n") < 0) {
                                                error_handler("dprintf()", errno);
                                        }
				}
			}

			if (pollfds[1].revents & POLLIN) {
				int num_read = read (pollfds[1].fd, buffer, BUFFER_SIZE);
                                if (num_read < 0) {
                                        error_handler("read()", errno);
                                }
				if (num_read == 0) {
					exit(0);
				}
				if (log_fd >= 0) {
                                        if (dprintf(log_fd, "RECEIVED %d bytes: ", num_read) < 0) {
						error_handler("dprintf()", errno);
					}
					if (write(log_fd, buffer, num_read) < 0) {
						error_handler("write()", errno);
					}
					if (dprintf(log_fd, "\n") < 0) {
                                                error_handler("dprintf()", errno);
                                        }
                                }
				if (compress_flag) {
                                        num_read = decompression(buffer, num_read);
                                }
                                for (int i = 0; i < num_read; i++) {
                                        if (buffer[i] == '\n') {
                                                char newline[2] = "\r\n";
                                                if (write(STDOUT_FILENO, newline, 2) < 0) {
                                                        error_handler("write()", errno);
                                                }
                                        } else {
                                                if (write(STDOUT_FILENO, buffer + i, 1) < 0) {
                                                        error_handler("write()", errno);
                                                }
                                        }
                                }

			}

			if (pollfds[0].revents & (POLLERR | POLLHUP) || pollfds[1].revents & (POLLERR | POLLHUP)) {
				exit(0);
			}
		}
	}	
}

int main(int argc, char** argv) {

	int port_no = -1;
	int sockfd;	

	struct option options[] = {
		{"port", 1, NULL, 'p'},
		{"log", 1, NULL, 'l'},
		{"compress", 0, NULL, 'c'}
	};
	int oc;
	while ((oc = getopt_long(argc, argv, "s", options, NULL)) != -1) {
		switch (oc) {
			case 'p':
				port_no = atoi(optarg);
				break;
			case 'l':
				log_fd = creat(optarg, 0644);
        			if (log_fd < 0) {
                			error_handler("creat()", errno);
        			}
				break;
			case 'c':
				compress_flag = 1;
				setup_compression();
				break;
			default:
				fprintf(stderr, "usage: lab1b-client --port=PORTNUMBER [--log=FILENAME] [--compress]\r\n");
				exit(1);
		}
	}
	if (port_no < 0) {
		fprintf(stderr, "usage: lab1b-client --port=PORTNUMBER [--log=FILENAME] [--compress]\r\n");
               	exit(1);
        }
	sockfd = socket_handler(port_no);

	pollfds[0].fd = STDIN_FILENO;
	pollfds[0].events = POLLIN;

	pollfds[1].fd = sockfd;
	pollfds[1].events = POLLIN;

	set_input_mode();
	keyboard_socket_listen();

  	exit(0);
}
