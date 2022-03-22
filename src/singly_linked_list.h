// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_SINGLY_LINKED_LIST_H
#define LIBPMEMSTREAM_SINGLY_LINKED_LIST_H

#include "stdint.h"

#include "libpmemstream_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Implementation based on PMDK SIMPLEQ list:
 * https://github.com/pmem/pmdk/blob/master/src/common/queue.h#L347
 *
 * This is part of multi-region allocator implementation to manage
 * allocation of data blocks and hold as allocated or free.
 */

struct singly_linked_list {
	uint64_t head;
	uint64_t tail;
};

#define SLIST_INVALID_OFFSET UINT64_MAX

#define SLIST_GET_PTR(type, runtime, it) (type *)(pmemstream_offset_to_ptr(runtime, it))
#define SLIST_NEXT(type, runtime, it, field) (SLIST_GET_PTR(type, runtime, it))->field

#define SLIST_INIT(list)                                                                                               \
	do {                                                                                                           \
		(list)->head = SLIST_INVALID_OFFSET;                                                                   \
		(list)->tail = SLIST_INVALID_OFFSET;                                                                   \
	} while (0)

#define SLIST_INSERT_HEAD(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		if ((list)->tail == SLIST_INVALID_OFFSET) {                                                            \
			(list)->tail = offset;                                                                         \
		}                                                                                                      \
		SLIST_NEXT(type, runtime, offset, field) = (list)->head;                                               \
		(list)->head = offset;                                                                                 \
	} while (0)

#define SLIST_INSERT_TAIL(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		SLIST_NEXT(type, runtime, offset, field) = SLIST_INVALID_OFFSET;                                       \
		if ((list)->head == SLIST_INVALID_OFFSET) {                                                            \
			(list)->head = offset;                                                                         \
		}                                                                                                      \
		if ((list)->tail != SLIST_INVALID_OFFSET) {                                                            \
			SLIST_NEXT(type, runtime, (list)->tail, field) = offset;                                       \
		}                                                                                                      \
		(list)->tail = offset;                                                                                 \
	} while (0)

#define SLIST_FOREACH(type, runtime, list, it, field)                                                                  \
	for ((it) = (list)->head; (it) != SLIST_INVALID_OFFSET; (it) = SLIST_NEXT(type, runtime, it, field))

#define SLIST_REMOVE_HEAD(type, runtime, list, field)                                                                  \
	do {                                                                                                           \
		if ((list)->head == SLIST_INVALID_OFFSET)                                                              \
			break;                                                                                         \
		if ((list)->tail == (list)->head)                                                                      \
			(list)->tail = SLIST_NEXT(type, runtime, (list)->head, field);                                 \
		(list)->head = SLIST_NEXT(type, runtime, (list)->head, field);                                         \
	} while (0)

#define SLIST_REMOVE(type, runtime, list, offset, field)                                                               \
	do {                                                                                                           \
		if ((list)->head == offset) {                                                                          \
			SLIST_REMOVE_HEAD(type, runtime, list, field);                                                 \
		} else {                                                                                               \
			uint64_t curelm = (list)->head;                                                                \
			while (SLIST_NEXT(type, runtime, curelm, field) != offset)                                     \
				curelm = SLIST_NEXT(type, runtime, curelm, field);                                     \
			if ((SLIST_NEXT(type, runtime, curelm, field) = SLIST_NEXT(type, runtime, offset, field)) ==   \
			    SLIST_INVALID_OFFSET)                                                                      \
				(list)->tail = curelm;                                                                 \
		}                                                                                                      \
	} while (0)

#ifdef __cplusplus
} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */
