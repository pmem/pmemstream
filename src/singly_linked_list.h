// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_SINGLY_LINKED_LIST_H
#define LIBPMEMSTREAM_SINGLY_LINKED_LIST_H

#include "stdint.h"

#include "libpmemstream_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

struct singly_linked_list {
	uint64_t head;
	uint64_t tail;
};

#define SLIST_INIT(list)                                                                                               \
	do {                                                                                                           \
		(list)->head = UINT64_MAX;                                                                             \
		(list)->tail = UINT64_MAX;                                                                             \
	} while (/*CONSTCOND*/ 0)

#define SLIST_INSERT_HEAD(runtime, list, offset, type, field)                                                          \
	do {                                                                                                           \
		if ((list)->tail == UINT64_MAX) {                                                                      \
			(list)->tail = offset;                                                                         \
		}                                                                                                      \
		((type *)(pmemstream_offset_to_ptr(runtime, offset)))->field = (list)->head;                           \
		(list)->head = offset;                                                                                 \
	} while (0)

#define SLIST_INSERT_TAIL(runtime, list, offset, type, field)                                                          \
	do {                                                                                                           \
		((type *)(pmemstream_offset_to_ptr(runtime, offset)))->field = UINT64_MAX;                             \
		if ((list)->head == UINT64_MAX) {                                                                      \
			(list)->head = (offset);                                                                       \
		}                                                                                                      \
		if ((list)->tail != UINT64_MAX) {                                                                      \
			((type *)(pmemstream_offset_to_ptr(runtime, (list)->tail)))->field = offset;                   \
		}                                                                                                      \
		(list)->tail = (offset);                                                                               \
	} while (0)

#define SLIST_FOREACH(type, runtime, list, it, field)                                                                  \
	for ((it) = (list)->head; (it) != UINT64_MAX; (it) = ((type *)(pmemstream_offset_to_ptr(runtime, it)))->field)

#define SLIST_NEXT(runtime, it, field) ((type *)(pmemstream_offset_to_ptr(runtime, it)))->field)

#define SLIST_REMOVE_HEAD(runtime, list, type, field)                                                                  \
	do {                                                                                                           \
		if ((list)->head == UINT64_MAX)                                                                        \
			break;                                                                                         \
		(list)->head = ((type *)(pmemstream_offset_to_ptr((runtime), (list)->head)))->field;                   \
	} while (0)

// ((node *)(pmemstream_offset_to_ptr(&runtime, it)))->data
#define SLIST_REMOVE(runtime, list, offset, type, field)                                                               \
	do {                                                                                                           \
		if ((list)->head == (offset)) {                                                                        \
			SLIST_REMOVE_HEAD(runtime, list, type, field);                                                 \
		} else {                                                                                               \
			uint64_t current_element = (list)->head;                                                       \
			uint64_t prev = 0;                                                                             \
			(void)prev;                                                                                    \
			while ((current_element =                                                                      \
					((type *)(pmemstream_offset_to_ptr(runtime, current_element)))->field) !=      \
			       offset) {                                                                               \
				prev = current_element;                                                                \
			}                                                                                              \
			((type *)(pmemstream_offset_to_ptr(runtime, prev)))->field =                                   \
				((type *)(pmemstream_offset_to_ptr(runtime, current_element)))->field;                 \
		}                                                                                                      \
	} while (0)
// void remove_elem(struct allocator_header *alloc_header, struct singly_linked_list *list, uint64_t it)

/*
if ((list)->head == (offset)) {                                                                        \
			SLIST_REMOVE_HEAD(runtime, list, type, field); \
		} else { \
			uint64_t curelm = (list)->head; \
			uint64_t prev = 0; \
			while ((current_element = \
					((type *)(pmemstream_offset_to_ptr(runtime, (list)->head)))->field) !=
it) {   \
				prev = current_element; \
				if ((curelm->field.sqe_next = curelm->field.sqe_next->field.sqe_next) == NULL) \
					(head)->sqh_last = &(curelm)->field.sqe_next; \
			} \
*/

#ifdef __cplusplus

} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */