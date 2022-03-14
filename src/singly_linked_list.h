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

#define INVALID_PTR UINT64_MAX

#define SLIST_NEXT(type, runtime, it, field) ((type *)(pmemstream_offset_to_ptr(runtime, it)))->field

#define SLIST_INIT(list)                                                                                               \
	do {                                                                                                           \
		(list)->head = INVALID_PTR;                                                                            \
		(list)->tail = INVALID_PTR;                                                                            \
	} while (/*CONSTCOND*/ 0)

#define SLIST_INSERT_HEAD(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		if ((list)->tail == INVALID_PTR) {                                                                     \
			(list)->tail = offset;                                                                         \
		}                                                                                                      \
		SLIST_NEXT(type, runtime, offset, field) = (list)->head;                                               \
		(list)->head = offset;                                                                                 \
	} while (0)

#define SLIST_INSERT_TAIL(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		SLIST_NEXT(type, runtime, offset, field) = INVALID_PTR;                                                \
		if ((list)->head == INVALID_PTR) {                                                                     \
			(list)->head = (offset);                                                                       \
		}                                                                                                      \
		if ((list)->tail != INVALID_PTR) {                                                                     \
			SLIST_NEXT(type, runtime, (list)->tail, field) = offset;                                       \
		}                                                                                                      \
		(list)->tail = (offset);                                                                               \
	} while (0)

#define SLIST_FOREACH(type, runtime, list, it, field)                                                                  \
	for ((it) = (list)->head; (it) != INVALID_PTR; (it) = SLIST_NEXT(type, runtime, it, field))

#define SLIST_REMOVE_HEAD(type, runtime, list, field)                                                                  \
	do {                                                                                                           \
		if ((list)->head == INVALID_PTR)                                                                       \
			break;                                                                                         \
		if ((list)->tail == (list)->head)                                                                      \
			(list)->tail = SLIST_NEXT(type, runtime, (list)->head, field);                                 \
		(list)->head = SLIST_NEXT(type, runtime, (list)->head, field);                                         \
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
			    INVALID_PTR)                                                                               \
				(list)->tail = INVALID_PTR;                                                            \
		}                                                                                                      \
	} while (0)

#ifdef __cplusplus
} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */
