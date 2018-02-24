/*
 * NAME: Lawrence Chen
 * ID: XXXXXXXXX
 * EMAIL: lawrencechen98@gmail.com
 */

#include <stdio.h>
#include <string.h>
#include <sched.h>
#include "SortedList.h"

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
	SortedListElement_t *cur = list->next;
	while((cur->key != NULL) && strcmp(element->key, cur->key) > 0) {
		if (opt_yield & INSERT_YIELD) {
			sched_yield();
		}
		cur = cur->next;
	}

	if (opt_yield & INSERT_YIELD) {
		sched_yield();
	}

	element->next = cur;
	element->prev = cur->prev;
	cur->prev->next = element;
	cur->prev= element;
}

int SortedList_delete(SortedListElement_t *element){
	if ((element->next->prev == element) && (element->prev->next == element)) {
		if (opt_yield & DELETE_YIELD) {
			sched_yield();
		}
		element->next->prev = element->prev;
		element->prev->next = element->next;
		return 0;
	} 
	return 1;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key){
	SortedListElement_t *cur = list->next;
	while(cur->key != NULL) {
		if (strcmp(key, cur->key) == 0)
			return cur;
		if (opt_yield & LOOKUP_YIELD) {
			sched_yield();
		}
		cur = cur->next;
	}
	return NULL;
}

int SortedList_length(SortedList_t *list){
	int count = 0;
	SortedListElement_t *cur = list->next;
	while(cur->key != NULL) {
		count = count + 1;
		if (opt_yield & LOOKUP_YIELD) {
			sched_yield();
		}
		if ((cur->next->prev != cur) || (cur->prev->next != cur))
			return -1;
		cur = cur->next;
	}
	return count;
}
