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

#define SLIST_INSERT_HEAD(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		if ((list)->tail == UINT64_MAX) {                                                                      \
			(list)->tail = offset;                                                                         \
		}                                                                                                      \
		((type *)(pmemstream_offset_to_ptr(runtime, offset)))->field = (list)->head;                           \
		(list)->head = offset;                                                                                 \
	} while (0)

#define SLIST_INSERT_TAIL(type, runtime, list, offset, field)                                                          \
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

#define SLIST_NEXT(type, runtime, it, field) ((type *)(pmemstream_offset_to_ptr(runtime, it)))->field

#define SLIST_REMOVE_HEAD(type, runtime, list, field)                                                                  \
	do {                                                                                                           \
		if ((list)->head == UINT64_MAX)                                                                        \
			break;                                                                                         \
		if ((list)->tail == (list)->head)                                                                      \
			(list)->tail = ((type *)(pmemstream_offset_to_ptr((runtime), (list)->head)))->field;           \
		(list)->head = ((type *)(pmemstream_offset_to_ptr((runtime), (list)->head)))->field;                   \
	} while (0)

#define SLIST_REMOVE(type, runtime, list, offset, field)                                                               \
	do {                                                                                                           \
		if ((list)->head == (offset)) {                                                                        \
			SLIST_REMOVE_HEAD(type, runtime, list, field);                                                 \
		} else {                                                                                               \
			uint64_t curelm = (list)->head;                                                                \
			while (SLIST_NEXT(type, runtime, curelm, field) != (offset))                                   \
				curelm = SLIST_NEXT(type, runtime, curelm, field);                                     \
			if ((SLIST_NEXT(type, runtime, curelm, field) = SLIST_NEXT(type, runtime, offset, field)) ==   \
			    UINT64_MAX)                                                                                \
				(list)->tail = UINT64_MAX;                                                             \
		}                                                                                                      \
	} while (0)

#ifdef __cplusplus
} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */