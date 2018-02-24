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

enum sync_type opt_sync = NO_LOCK;
int opt_yield = 0;
int num_iter = 1;
int num_threads = 1;
int num_elements = 1;
int num_lists = 1;

char* yield_opts = NULL;
char* sync_opts = NULL;

typedef struct SubList {
	SortedList_t list;
	pthread_mutex_t mutex;
	int lock;
} SubList_t;

SubList_t* shared_lists;
SortedListElement_t* shared_elements;
long long* wait_times;

void clean_up() {
	if (opt_sync == MUTEX) {
		for (int i = 0; i < num_lists; i++)
			pthread_mutex_destroy(&shared_lists[i].mutex);
	}
	if (yield_opts)
		free(yield_opts);
	if (shared_elements) {
		for (int i = 0; i < num_elements; i++) {
			if (shared_elements[i].key)
				free((void*) shared_elements[i].key);
		}
		free(shared_elements);
	}
	if (shared_lists)
		free(shared_lists);
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
	struct timespec start;
	struct timespec end;

	for (int i = *tid; i < num_elements; i += num_threads) {
		int hash = (shared_elements[i].key[0] + shared_elements[i].key[1] + shared_elements[i].key[2]);
		int bucket = abs(hash % num_lists);
		switch (opt_sync) {
			case NO_LOCK: 
				SortedList_insert(&shared_lists[bucket].list, &shared_elements[i]);
				break;
			case MUTEX:
				if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
					error_handler("clock_gettime()", errno);
				}
				pthread_mutex_lock(&shared_lists[bucket].mutex);
				if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
					error_handler("clock_gettime()", errno);
				}
				wait_times[(int) *tid] += ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
				SortedList_insert(&shared_lists[bucket].list, &shared_elements[i]);
				pthread_mutex_unlock(&shared_lists[bucket].mutex);
				break;
			case SPIN_LOCK:
				if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
					error_handler("clock_gettime()", errno);
				}
				while (__sync_lock_test_and_set(&shared_lists[bucket].lock, 1) == 1) ;
				if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
					error_handler("clock_gettime()", errno);
				}
				wait_times[(int) *tid] += ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
				SortedList_insert(&shared_lists[bucket].list, &shared_elements[i]);
				__sync_lock_release (&shared_lists[bucket].lock);	    
				break;
		}
	}

	switch (opt_sync) {
		case MUTEX:
			if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
				error_handler("clock_gettime()", errno);
			}
			for (int i = 0; i < num_lists; i++) 
				pthread_mutex_lock(&shared_lists[i].mutex);
			if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
				error_handler("clock_gettime()", errno);
			}
			wait_times[(int) *tid] += ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
			break;
		case SPIN_LOCK:
			if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
				error_handler("clock_gettime()", errno);
			}
			for (int i = 0; i < num_lists; i++) 
				while (__sync_lock_test_and_set(&shared_lists[i].lock, 1) == 1) ;  
			if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
				error_handler("clock_gettime()", errno);
			}
			wait_times[(int) *tid] += ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
			break;
		default:
			break;
	}

	int length = 0;
	for (int i = 0; i < num_lists; i++) {
		int sub_length = SortedList_length(&shared_lists[i].list);
		if (sub_length < 0) {
			fprintf(stderr, "Error: Corrupted list detected while getting length.\n");
			exit(2);
		}
		length += sub_length;
	}
	if (length < 0) {
		fprintf(stderr, "Error: Corrupted list detected while getting length.\n");
		exit(2);
	}

	switch (opt_sync) {
		case MUTEX:
			for (int i = 0; i < num_lists; i++) 
				pthread_mutex_unlock(&shared_lists[i].mutex);
			break;
		case SPIN_LOCK:
			for (int i = 0; i < num_lists; i++) 
				__sync_lock_release (&shared_lists[i].lock);	    
			break;
		default:
			break;
	}


	SortedListElement_t *retrieved;
	for (int i = *tid; i < num_elements; i += num_threads) {
		int bucket = abs((shared_elements[i].key[0] + shared_elements[i].key[1] + shared_elements[i].key[2]) % num_lists);
		switch (opt_sync) {
			case NO_LOCK: 
				retrieved = SortedList_lookup(&shared_lists[bucket].list, shared_elements[i].key);
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
				if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
					error_handler("clock_gettime()", errno);
				}
				pthread_mutex_lock(&shared_lists[bucket].mutex);
				if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
					error_handler("clock_gettime()", errno);
				}
				wait_times[(int) *tid] += ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
				retrieved = SortedList_lookup(&shared_lists[bucket].list, shared_elements[i].key);
				if (retrieved == NULL) {
					fprintf(stderr, "Error: Corrupted list. Specified element key not found.\n");
					exit(2);
				}
				if (SortedList_delete(retrieved) == 1) {
					fprintf(stderr, "Error: Corrupted list detected while deleting.\n");
					exit(2);
				}
				pthread_mutex_unlock(&shared_lists[bucket].mutex);
				break;
			case SPIN_LOCK:
				if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
					error_handler("clock_gettime()", errno);
				}
				while (__sync_lock_test_and_set(&shared_lists[bucket].lock, 1) == 1) ;
				if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
					error_handler("clock_gettime()", errno);
				}
				wait_times[(int) *tid] += ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
				retrieved = SortedList_lookup(&shared_lists[bucket].list, shared_elements[i].key);
				if (retrieved == NULL) {
					fprintf(stderr, "Error: Corrupted list. Specified element key not found.\n");
					exit(2);
				}
				if (SortedList_delete(retrieved) == 1) {
					fprintf(stderr, "Error: Corrupted list detected while deleting.\n");
					exit(2);
				}
				__sync_lock_release (&shared_lists[bucket].lock);	    
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
		{"sync", 1, NULL, 's'},
		{"lists", 1, NULL, 'l'},
		{0,0,0,0}
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
			case 'l':
				num_lists = (int)atoi(optarg);
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
		char* key = (char*) malloc(sizeof(char) * 4);
		key[0] = (char)(rand() % 256);
		key[1] = (char)(rand() % 256);
		key[2] = (char)(rand() % 256);
		key[3] = '\0';
		shared_elements[i].key = key;
	}

	shared_lists = (SubList_t*) malloc(sizeof(SubList_t) * num_lists);
	for (int i = 0; i < num_lists; i++) {
		shared_lists[i].list.next = &shared_lists[i].list;
		shared_lists[i].list.prev = &shared_lists[i].list;
		shared_lists[i].list.key = NULL;
		if (opt_sync == MUTEX) 
			pthread_mutex_init(&shared_lists[i].mutex, NULL);
	}

	pthread_t threads[num_threads];
	int tids[num_threads];
	wait_times = (long long*) malloc(sizeof(long long) * num_threads);

	struct timespec start_time;
	if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
		error_handler("clock_gettime()", errno);
	}

	for(int i = 0; i < num_threads; i++) {
		tids[i] = i;
		wait_times[i] = 0;
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

    int length = 0;
	for (int i = 0; i < num_lists; i++) {
		int sub_length = SortedList_length(&shared_lists[i].list);
		if (sub_length < 0) {
			fprintf(stderr, "Error: Corrupted list detected while getting length.\n");
			exit(2);
		}
		length += sub_length;
	}
	if (length != 0) {
		fprintf(stderr, "Error: Corrupted list. Final list length is not 0.\n");
		exit(2);
	}

	long long total_wait_time = 0;
	for (int i = 0; i < num_threads; i++) 
		total_wait_time += wait_times[i];
	int num_locks = (2 * num_iter + 1) * num_threads;

	long long elapsed_time_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
	int num_ops = 3 * num_threads * num_iter;

	fprintf(stdout, "list%s%s,%d,%d,%d,%d,%lld,%lld,%lld\n", yield_opts, sync_opts, num_threads, num_iter, num_lists, num_ops, elapsed_time_ns, elapsed_time_ns/num_ops, total_wait_time/num_locks);

	exit(0);
}
