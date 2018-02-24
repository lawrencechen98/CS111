/*
NAME: Lawrence Chen
EMAIL: lawrencechen98@gmail.com
ID: XXXXXXXXX
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

//save old terminal attributes
struct termios saved_attributes;

//for inter-process communication with pipe()
int to_pipefd[2];
int from_pipefd[2];
pid_t child_pid = -1;

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
	exit(0);
}

//function to make sure final inputs in the pipe from shell output is processed before exiting
void final_pipe_output() {
	while(1) {
		if (pollfds[1].revents & POLLIN) {
			int num_read = read (from_pipefd[0], buffer, BUFFER_SIZE);
			if (num_read <= 0) {
				break;
			}
			for (int i = 0; i < num_read; i++) {
				if (buffer[i] == '\n') {
					char newline[2] = "\r\n";
					if (write(STDOUT_FILENO, newline, 2) < 0) {
						error_handler("write()", errno);
					}
				} else if (buffer[i] == 4) {
					char EOT[2] = "^D";
					if (write(STDOUT_FILENO, EOT, 2) < 0) {
						error_handler("write()", errno);
					}
					break;
				} else {
					if (write(STDOUT_FILENO, buffer + i, 1) < 0) {
						error_handler("write()", errno);
					}
				}
			}
		}
		
		if (pollfds[0].revents & (POLLERR | POLLHUP) || pollfds[1].revents & (POLLERR | POLLHUP)) {
			break;
		}
	}

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
		
		pollfds[0].fd = STDIN_FILENO;
		pollfds[0].events = POLLIN;

		pollfds[1].fd = from_pipefd[0];
		pollfds[1].events = POLLIN;

		if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
			error_handler("signal()", errno);
		}
		if (atexit(final_pipe_output) == -1) {
			error_handler("atexit()", errno);
		}
		
	} else if (child_pid == 0) {
		close_with_error_check(to_pipefd[1]);
		close_with_error_check(from_pipefd[0]);

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

//function to run main I/O loop for when --shell flag is indicated
void run_shell() {
	fork_shell();

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
						if (write(to_pipefd[1], newline+1, 1) < 0) {
							error_handler("write()", errno);
						}
					} else if (buffer[i] == 4) {
						char EOT[2] = "^D";
						if (write(STDOUT_FILENO, EOT, 2) < 0) {
							error_handler("write()", errno);
						}
						close(to_pipefd[1]);
					} else if (buffer[i] == 3) {
						char ETX[2] = "^C";
						if (write(STDOUT_FILENO, ETX, 2) < 0) {
							error_handler("write()", errno);
						}
						kill(child_pid, SIGINT);
					} else {
						if (write(STDOUT_FILENO, buffer + i, 1) < 0) {
							error_handler("write()", errno);
						}
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
                                        if (buffer[i] == '\n') {
                                                char newline[2] = "\r\n";
                                                if (write(STDOUT_FILENO, newline, 2) < 0) {
                                                        error_handler("write()", errno);
                                                }
                                        } else if (buffer[i] == 4) {
                                                char EOT[2] = "^D";
                                                if (write(STDOUT_FILENO, EOT, 2) < 0) {
                                                        error_handler("write()", errno);
                                                }
						exit(0);
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

//function to run default main I/O loop
void run_normal() {
	while(1){
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
			} else if (buffer[i] == 4) {
				char EOT[2] = "^D";
				if (write(STDOUT_FILENO, EOT, 2) < 0) {
					error_handler("write()", errno);
				}
				exit(0);
			} else if (buffer[i] == 3) {
				char ETX[2] = "^C";
				if (write(STDOUT_FILENO, ETX, 2) < 0) {
					error_handler("write()", errno);
				}
			} else {
				if (write(STDOUT_FILENO, buffer + i, 1) < 0) {
					error_handler("write()", errno);
				}
			}
		}
	}
}

//function to reset original terminal attributes
void reset_input_mode() {
       tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}

//function to set terminal input mode to non-canonical and no echo
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

//main function
int main(int argc, char** argv) {

  	set_input_mode();

	struct option options[] = {
		{"shell", 0, NULL, 's'}
	};
	int oc;
	int shell_flag = 0;
	while ((oc = getopt_long(argc, argv, "s", options, NULL)) != -1) {
		
		if (oc == 's') {
			shell_flag = 1;
			break;
		} else {
			fprintf(stderr, "usage: lab1a [--shell]\n");
			exit(1);
		}
	}

	if (shell_flag) {
		run_shell();
	} else {
		run_normal();
	}

  	exit(0);
}
