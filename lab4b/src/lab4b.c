/*
NAME: Lawrence Chen
EMAIL: lawrencechen98@gmail.com
ID: XXXXXXXXX
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <mraa.h>
#include <math.h>

#define BUFFER_SIZE 1024

int shutdown = 0;
int enabled = 1;
int period = 1;
char scale = 'F';
int logfd;

mraa_aio_context temp_sensor;
mraa_gpio_context button;

void error_handler(char* func, int err) {
	fprintf(stderr, "Error in %s: (%d) %s\n", func, err, strerror(err));
	exit(1);
}

void button_handler() {
	shutdown = 1;
}

void invalid_command_handler(char* invalid) {
	fprintf(stderr, "Error: Invalid command received in STDIN, \"%s\"\n", invalid);
	exit(1);
}

void process_command(char* command) {
	if (strncmp(command, "SCALE=", 6*sizeof(char)) == 0) {
		if (command[6] == 'F')
			scale = 'F';
		else if (command[6] == 'C')
			scale = 'C';
		else
			invalid_command_handler(command);

    } else if (strncmp(command, "PERIOD=", 7*sizeof(char)) == 0) {
        period = (int)atoi(command+7);

    } else if (strcmp(command, "STOP") == 0) {
    	enabled = 0;

    } else if (strcmp(command, "START") == 0) {
        enabled = 1;

    } else if (strcmp(command, "OFF") == 0) {
    	shutdown = 1;

    } else if (!(strncmp(command, "LOG", 3*sizeof(char)) == 0)){
    	invalid_command_handler(command);
    }

    if (logfd)
    	dprintf(logfd, "%s\n", command);
}

float read_temp() {
	int reading = mraa_aio_read(temp_sensor);

	int B = 4275; // thermistor value
	float R0 = 100000.0; // nominal base value
	float R = 1023.0/((float) reading) - 1.0;
	R = R0*R;

	float C = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;

	return (scale == 'F') ? (C * 9)/5 + 32 : C;
}

int main(int argc, char **argv) {

	struct option options[] = {
		{"period", 1, NULL, 'p'},
		{"scale", 1, NULL, 's'},
		{"log", 1, NULL, 'l'},
		{0,0,0,0}
	};

	int oc;
	while ((oc = getopt_long(argc, argv, "p:s:l:", options, NULL)) != -1) {
		switch(oc) {
			case 'p':
				period = (int)atoi(optarg);
				break;
			case 's':
				if (*optarg == 'F' || *optarg == 'C') {
                    scale = *optarg;
                } else {
                	fprintf(stderr, "usage: lab4b [--period=#] [--scale=[CF]] [--log=FILENAME]\n");
					exit(1);
                }
				break;
			case 'l':
				logfd = creat(optarg, 0666);
				if (logfd < 0) 
					error_handler("creat()", errno);
				break;
			default:
				fprintf(stderr, "usage: lab4b [--period=#] [--scale=[CF]] [--log=FILENAME]\n");
				exit(1);
		}
	}

	temp_sensor = mraa_aio_init(1);
	button = mraa_gpio_init(60);
	mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &button_handler, NULL);

	struct timeval clock;
	struct tm *now;
	time_t next_sample = 0;

	struct pollfd poll_stdin = {0, POLLIN, 0};

	char buffer[BUFFER_SIZE];
	char cache[BUFFER_SIZE];

	memset(buffer, 0, BUFFER_SIZE);
	memset(cache, 0, BUFFER_SIZE);

	int index = 0;

	while (!shutdown) {

		if (gettimeofday(&clock, 0))
			error_handler("gettimeofday()", errno);
		if (enabled && clock.tv_sec >= next_sample) {
			float temp = read_temp();
			int temp_dec = temp * 10;

			now = localtime(&(clock.tv_sec));
			fprintf(stdout, "%02d:%02d:%02d %d.%1d\n", now->tm_hour, now->tm_min, now->tm_sec, temp_dec/10, temp_dec%10);
			if (logfd)
				dprintf(logfd, "%02d:%02d:%02d %d.%1d\n", now->tm_hour, now->tm_min, now->tm_sec, temp_dec/10, temp_dec%10);
		
			next_sample = clock.tv_sec + period;
		}

		int ret = poll(&poll_stdin, 1, 0);
		if (ret < 0) {
			error_handler("poll()", errno);
		}
		if (ret > 0) {
			if (poll_stdin.revents & POLLIN) {
				int num_read = read(STDIN_FILENO, buffer, BUFFER_SIZE);
				if (num_read < 0) {
					error_handler("read()", errno);
				}

				for (int i = 0; i < num_read && index < BUFFER_SIZE; i++) {
					if (buffer[i] == '\n') {
						process_command((char*)&cache);
						index = 0;
						memset(cache, 0, BUFFER_SIZE);
					} else {
						cache[index] = buffer[i];
						index++;
					}
				}
			}
		}
	}

	now = localtime(&(clock.tv_sec));
	fprintf(stdout, "%02d:%02d:%02d SHUTDOWN\n", now->tm_hour, now->tm_min, now->tm_sec);
	if (logfd)
		dprintf(logfd, "%02d:%02d:%02d SHUTDOWN\n", now->tm_hour, now->tm_min, now->tm_sec);

	mraa_aio_close(temp_sensor);
	mraa_gpio_close(button);
	exit(0);
}
