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
#include "SortedList.h"
#include <signal.h>


enum sync_type {NO_LOCK, MUTEX, SPIN_LOCK};

int opt_yield = 0;
int num_iter = 1;
int num_threads = 1;
int num_elements = 1;

char* yield_opts = NULL;
char* sync_opts = NULL;

enum sync_type opt_sync = NO_LOCK;
pthread_mutex_t mutex;
int lock;

SortedList_t* shared_list;
SortedListElement_t* shared_elements;

void clean_up() {
	if (opt_sync == MUTEX)
		pthread_mutex_destroy(&mutex);
	if (yield_opts)
		free(yield_opts);
	if (shared_elements)
		free(shared_elements);
	if (shared_list)
		free(shared_list);

}

void signal_handler(int sig) {
    if (sig == SIGSEGV) {
        fprintf(stderr, "Error: Signal %d SIGSEGV was caught, %s\n", sig, strsignal(sig));
        exit(2);
    }
}

void error_handler(char* func, int err) {
	fprintf(stderr, "Error in %s: (%d)%s\n", func, err, strerror(err));
	exit(1);
}

void* list_thread(void *arg) {
	int* tid = (int *) arg;

	for (int i = *tid; i < num_elements; i += num_threads) {
		switch (opt_sync) {
			case NO_LOCK: 
				SortedList_insert(shared_list, &shared_elements[i]);
				break;
			case MUTEX:
				pthread_mutex_lock(&mutex);
				SortedList_insert(shared_list, &shared_elements[i]);
				pthread_mutex_unlock(&mutex);
				break;
			case SPIN_LOCK:
				while (__sync_lock_test_and_set(&lock, 1) == 1) ;
				SortedList_insert(shared_list, &shared_elements[i]);
				__sync_lock_release (&lock);	    
				break;
		}
	}

	switch (opt_sync) {
		case NO_LOCK: 
			if (SortedList_length(shared_list) < 0) {
				fprintf(stderr, "Error: Corrupted list detected while getting length.\n");
				exit(2);
			}
			break;
		case MUTEX:
			pthread_mutex_lock(&mutex);
			if (SortedList_length(shared_list) < 0) {
				fprintf(stderr, "Error: Corrupted list detected while getting length.\n");
				exit(2);
			}
			pthread_mutex_unlock(&mutex);
			break;
		case SPIN_LOCK:
			while (__sync_lock_test_and_set(&lock, 1) == 1) ;
			if (SortedList_length(shared_list) < 0) {
				fprintf(stderr, "Error: Corrupted list detected while getting length.\n");
				exit(2);
			}
			__sync_lock_release (&lock);	    
			break;
	}


	SortedListElement_t *retrieved;
	for (int i = *tid; i < num_elements; i += num_threads) {
		switch (opt_sync) {
			case NO_LOCK: 
				retrieved = SortedList_lookup(shared_list, shared_elements[i].key);
				if (retrieved == NULL) {
					fprintf(stderr, "Error: Corrupted list. Specified element key not found.\n");
					exit(2);
				}
				if (SortedList_delete(retrieved) == 1) {
					fprintf(stderr, "Error: Corrupted list detected while deleting.\n");
					exit(2);
				}
				break;
			case MUTEX:
				pthread_mutex_lock(&mutex);
				retrieved = SortedList_lookup(shared_list, shared_elements[i].key);
				if (retrieved == NULL) {
					fprintf(stderr, "Error: Corrupted list. Specified element key not found.\n");
					exit(2);
				}
				if (SortedList_delete(retrieved) == 1) {
					fprintf(stderr, "Error: Corrupted list detected while deleting.\n");
					exit(2);
				}
				pthread_mutex_unlock(&mutex);
				break;
			case SPIN_LOCK:
				while (__sync_lock_test_and_set(&lock, 1) == 1) ;
				retrieved = SortedList_lookup(shared_list, shared_elements[i].key);
				if (retrieved == NULL) {
					fprintf(stderr, "Error: Corrupted list. Specified element key not found.\n");
					exit(2);
				}
				if (SortedList_delete(retrieved) == 1) {
					fprintf(stderr, "Error: Corrupted list detected while deleting.\n");
					exit(2);
				}
				__sync_lock_release (&lock);	    
				break;
		}
	}

	return NULL;
}

int main(int argc, char **argv) {
	if (atexit(clean_up) == -1) {
		error_handler("atexit()", errno);
	}

	if (signal(SIGSEGV, signal_handler) == SIG_ERR) {
		error_handler("signal()", errno);
	}

	struct option options[] = {
		{"yield", 1, NULL, 'y'},
		{"iterations", 1, NULL, 'i'},
		{"threads", 1, NULL, 't'},
		{"sync", 1, NULL, 's'}
	};

	int oc;
	int optnum;
	while ( (oc = getopt_long(argc, argv, "i:o:sc", options, NULL)) != -1) {
		switch(oc) {
			case 'y':
				optnum = strlen(optarg);
                for (int i = 0; i < optnum; i++) {
                    switch (optarg[i]) {
                        case 'i': {
                            opt_yield = opt_yield | INSERT_YIELD;
                            break;
                        }
                        case 'd': {
                            opt_yield = opt_yield | DELETE_YIELD;
                            break;
                        }
                        case 'l': {
                            opt_yield = opt_yield | LOOKUP_YIELD;
                            break;
                        }
                        default: {
                            fprintf(stderr, "usage: lab2_list [--yield=[idl]] [--iterations=#] [--threads=#] [--sync=[ms]]\n");
							exit(1);
                        }
                    }
                }
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
                    default: {
                        fprintf(stderr, "usage: lab2_list [--yield=[idl]] [--iterations=#] [--threads=#] [--sync=[ms]]\n");
						exit(1);
                    }
				}
				break;
			default:
				fprintf(stderr, "usage: lab2_list [--yield=[idl]] [--iterations=#] [--threads=#] [--sync=[ms]]\n");
				exit(1);
		}
	}

	yield_opts = (char*) malloc(6);

	sprintf(yield_opts, "-");
	if (opt_yield == 0) 
		sprintf(yield_opts, "%s%s", yield_opts, "none");
	else {
		if (opt_yield & INSERT_YIELD)
			sprintf(yield_opts, "%s%s", yield_opts, "i");
		if (opt_yield & DELETE_YIELD)
			sprintf(yield_opts, "%s%s", yield_opts, "d");
		if (opt_yield & LOOKUP_YIELD)
			sprintf(yield_opts, "%s%s", yield_opts, "l");
	}

	switch (opt_sync) {
		case NO_LOCK:
			sync_opts = "-none";
			break;
		case MUTEX:
			sync_opts = "-m";
			break;
		case SPIN_LOCK:
			sync_opts = "-s";
			break;
	}

	num_elements = num_iter * num_threads;
	shared_elements = (SortedListElement_t*) malloc(sizeof(SortedListElement_t) * num_elements);
	srand(time(NULL));
	for (int i = 0; i < num_elements; i++) {
		char key[3];
		key[0] = (char)(rand() % 256);
		key[1] = (char)(rand() % 256);
		key[2] = '\0';
		shared_elements[i].key = key;
	}

	shared_list = (SortedList_t*) malloc(sizeof(SortedList_t));
	shared_list->next = shared_list;
	shared_list->prev = shared_list;

	pthread_t threads[num_threads];
	int tids[num_threads];

	struct timespec start_time;
	if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
		error_handler("clock_gettime()", errno);
	}

	for(int i = 0; i < num_threads; i++) {
		tids[i] = i;
		int err = pthread_create(&threads[i], NULL, list_thread, &tids[i]);
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

	if (SortedList_length(shared_list) != 0) {
		fprintf(stderr, "Error: Corrupted list. Final list length is not 0.\n");
        exit(2);
    }

	long long elapsed_time_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
	int num_ops = 3 * num_threads * num_iter;
	fprintf(stdout, "list%s%s,%d,%d,%d,%d,%lld,%lld\n", yield_opts, sync_opts, num_threads, num_iter, 1, num_ops, elapsed_time_ns, elapsed_time_ns/num_ops);

	exit(0);
}
