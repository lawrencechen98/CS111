/*
NAME: Lawrence Chen
EMAIL: lawrencechen98@gmail.com
ID: XXXXXXXXX
*/

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

void catchHandler(int sig) {
    if (sig == SIGSEGV) {
        fprintf(stderr, "Error: Signal %d SIGSEGV was caught, %s\n", sig, strsignal(sig));
        exit(4);
    }
}

void segfaultHandler() {
	char* nullptr = NULL;
	*nullptr = 0;
}

void copy(int in_fd, int out_fd) {
	char buffer;
	int read_num = read(in_fd, &buffer, 1);
	while(read_num > 0){
		if (write(out_fd, &buffer, 1) == -1) {
			fprintf(stderr, "Error writing to output with file descriptor %d: %s\n", out_fd, strerror(errno));
			exit(3);
		}
		read_num = read(in_fd, &buffer, 1);
	}
	if (read_num == -1) {
		fprintf(stderr, "Error reading from input with file descriptor %d: %s\n", in_fd, strerror(errno));
		exit(2);
	}
	exit(0);
}

int main(int argc, char** argv) {
	struct option options[] = {
		{"input", 1, NULL, 'i'},
		{"output", 1, NULL, 'o'},
		{"segfault", 0, NULL, 's'},
		{"catch", 0, NULL, 'c'}
	};

	int oc;

	char* in_filename = NULL;
	int in_fd = 0;
	char* out_filename = NULL;
	int out_fd = 1;

	int s_flag = 0;
	int c_flag = 0;


	while ( (oc = getopt_long(argc, argv, "i:o:sc", options, NULL)) != -1) {
		switch(oc) {
			case 'i':
				in_filename = optarg;
				break;
			case 'o':
				out_filename = optarg;
				break;
			case 's':
				s_flag = 1;
				break;
			case 'c':
				c_flag = 1;
				break;
			default:
				fprintf(stderr, "usage: lab0 [--input=FILENAME] [--output=FILENAME] [--segfault] [--catch]\n");
				exit(1);
		}
	}

	if (in_filename) {
		in_fd = open(in_filename, O_RDONLY);
		if (in_fd >= 0) {
			close(0);
			dup(in_fd);
			close(in_fd);
		} else {
			fprintf(stderr, "Error with --input option \"%s\": %s\n", in_filename, strerror(errno));
			exit(2);
		}
	}

	if (out_filename) {
	    out_fd = creat(out_filename, 0666);
	    if (out_fd >= 0) {
	        close(1);
	        dup(out_fd);
	        close(out_fd);
	    } else {
	        fprintf(stderr, "Error with --output option \"%s\": %s\n", out_filename, strerror(errno));
	        exit(3);
	    }
	}

	if (c_flag) {
		if (signal(SIGSEGV, catchHandler) == SIG_ERR) {
            fprintf(stderr, "Error with --catch option: error using signal() to register a segmentation fault handler");
        }
	}

	if (s_flag) {
		segfaultHandler();
	}

	copy(0, 1);

}
