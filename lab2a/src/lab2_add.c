/*
NAME: Lawrence Chen
EMAIL: lawrencechen98@gmail.com
ID: XXXXXXXXX
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

enum sync_type {NO_LOCK, MUTEX, SPIN_LOCK, COMPARE_SWAP};

int opt_yield = 0;
int num_iter = 1;
int num_threads = 1;

char* test_name = NULL;

enum sync_type opt_sync = NO_LOCK;
pthread_mutex_t mutex;
int lock;

void clean_up() {
	if (opt_sync == MUTEX)
		pthread_mutex_destroy(&mutex);
}

void error_handler(char* func, int err) {
	fprintf(stderr, "Error in %s: (%d)%s\n", func, err, strerror(err));
	exit(1);
}

void add(long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (opt_yield)
		sched_yield();
	*pointer = sum;
}

void add_with_compare(long long *pointer, long long value) {
	long long cur;
	long long new;
	do {
		cur = *pointer;
		new = cur + value;
		if(opt_yield)
			sched_yield();
	} while(__sync_val_compare_and_swap(pointer, cur, new) != cur);
}

void* add_thread(void *counter_ptr) {
	long long *pointer = (long long *) counter_ptr;

	for (int i = 0; i < num_iter; i++) {
		switch (opt_sync) {
			case NO_LOCK:
				add(pointer, 1);
				break;
			case MUTEX:
				pthread_mutex_lock(&mutex);
				add(pointer, 1);
				pthread_mutex_unlock(&mutex);
				break;
			case SPIN_LOCK:
				while (__sync_lock_test_and_set(&lock, 1));
				add(pointer, 1);
				__sync_lock_release(&lock);
				break;
			case COMPARE_SWAP:
				add_with_compare(pointer, 1);
				break;
		}
	}

	for (int i = 0; i < num_iter; i++) {
		switch (opt_sync) {
			case NO_LOCK:
				add(pointer, -1);
				break;
			case MUTEX:
				pthread_mutex_lock(&mutex);
				add(pointer, -1);
				pthread_mutex_unlock(&mutex);
				break;
			case SPIN_LOCK:
				while (__sync_lock_test_and_set(&lock, 1));
				add(pointer, -1);
				__sync_lock_release(&lock);
				break;
			case COMPARE_SWAP:
				add_with_compare(pointer, -1);
				break;
		}
	}

	return NULL;
}

int main(int argc, char **argv) {
	if (atexit(clean_up) == -1) {
		error_handler("atexit()", errno);
	}

	struct option options[] = {
		{"yield", 0, NULL, 'y'},
		{"iterations", 1, NULL, 'i'},
		{"threads", 1, NULL, 't'},
		{"sync", 1, NULL, 's'}
	};

	int oc;
	while ( (oc = getopt_long(argc, argv, "i:o:sc", options, NULL)) != -1) {
		switch(oc) {
			case 'y':
				opt_yield = 1;
				break;
			case 'i':
				num_iter = (int)atoi(optarg);
				break;
			case 't':
				num_threads = (int)atoi(optarg);
				break;
			case 's':
				switch (*optarg) {
                    case 'm': {
                        opt_sync = MUTEX;
                        pthread_mutex_init(&mutex, NULL);
                        break;
                    }
                    case 's': {
                        opt_sync = SPIN_LOCK;
                        break;
                    }
                    case 'c': {
                        opt_sync = COMPARE_SWAP;
                        break;
                    }
                    default: {
                        fprintf(stderr, "usage: lab2_add [--yield] [--iterations=#] [--threads=#] [--sync=[msc]]\n");
						exit(1);
                    }
				}
				break;
			default:
				fprintf(stderr, "usage: lab2_add [--yield] [--iterations=#] [--threads=#] [--sync=[msc]]\n");
				exit(1);
		}
	}

    if (opt_yield) {
    	switch (opt_sync) {
    		case NO_LOCK:
    			test_name = "add-yield-none";
    			break;
    		case MUTEX:
    			test_name = "add-yield-m";
    			break;
    		case SPIN_LOCK:
    			test_name = "add-yield-s";
    			break;
    		case COMPARE_SWAP:
    			test_name = "add-yield-c";
    			break;
    	}
    } else {
    	switch (opt_sync) {
    		case NO_LOCK:
    			test_name = "add-none";
    			break;
    		case MUTEX:
    			test_name = "add-m";
    			break;
    		case SPIN_LOCK:
    			test_name = "add-s";
    			break;
    		case COMPARE_SWAP:
    			test_name = "add-c";
    			break;
    	}
    }

	pthread_t threads[num_threads];
    long long counter = 0;

	struct timespec start_time;
	if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
		error_handler("clock_gettime()", errno);
	}

	for(int i = 0; i < num_threads; i++) {
		int err = pthread_create(&threads[i], NULL, add_thread, &counter);
		if (err) {
			fprintf(stderr, "Error in pthread_create(): (%d)%s\n", err, strerror(err));
        		exit(2);
		}
	}

	for(int i = 0; i < num_threads; i++) {
		int err = pthread_join(threads[i], NULL);
		if (err) {
			fprintf(stderr, "Error in pthread_join(): (%d)%s\n", err, strerror(err));
                        exit(2);
		}
	}

	struct timespec end_time;
	if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
		error_handler("clock_gettime()", errno);
	}

	long long elapsed_time_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
	int num_ops = 2 * num_threads * num_iter;
	fprintf(stdout, "%s,%d,%d,%d,%lld,%lld,%lld\n", test_name, num_threads, num_iter, num_ops, elapsed_time_ns, elapsed_time_ns/num_ops, counter);

	exit(0);
}
