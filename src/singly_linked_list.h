// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_SINGLY_LINKED_LIST_H
#define LIBPMEMSTREAM_SINGLY_LINKED_LIST_H

#include "assert.h"
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
#define SLIST_NEXT(type, runtime, it, field) ((SLIST_GET_PTR(type, runtime, it))->field)

static inline void store_with_flush(const pmemstream_data_runtime *runtime, uint64_t *src, uint64_t value)
{
	__atomic_store_n(src, value, __ATOMIC_RELAXED);
	(runtime)->flush(src, sizeof(*src));
}

#define SLIST_INVARIANT_HEAD(type, runtime, list, field)                                                               \
	(((list)->head != SLIST_INVALID_OFFSET) ||                                                                     \
	 ((list)->head == SLIST_INVALID_OFFSET && (list)->tail == SLIST_INVALID_OFFSET))
#define SLIST_INVARIANT_TAIL(type, runtime, list, field)                                                               \
	(((list)->tail != SLIST_INVALID_OFFSET) ||                                                                     \
	 ((list)->head == SLIST_INVALID_OFFSET && (list)->tail == SLIST_INVALID_OFFSET))
#define SLIST_INVARIANT_TAIL_NEXT(type, runtime, list, field)                                                          \
	(((list)->tail == SLIST_INVALID_OFFSET) ||                                                                     \
	 ((SLIST_NEXT(type, runtime, (list)->tail, field) == SLIST_INVALID_OFFSET) &&                                  \
	  (((list)->head == SLIST_INVALID_OFFSET))))
#define SLIST_INVARIANTS(type, runtime, list, field)                                                                   \
	(SLIST_INVARIANT_HEAD(type, runtime, list, field) && SLIST_INVARIANT_TAIL(type, runtime, list, field) &&       \
	 SLIST_INVARIANT_TAIL_NEXT(type, runtime, list, field))

#define SLIST_INIT(runtime, list)                                                                                      \
	do {                                                                                                           \
		store_with_flush(runtime, &(list)->tail, SLIST_INVALID_OFFSET);                                        \
		store_with_flush(runtime, &(list)->head, SLIST_INVALID_OFFSET);                                        \
		(runtime)->drain();                                                                                    \
	} while (0)

#define SLIST_RUNTIME_INIT(type, runtime, list, field)                                                                 \
	do {                                                                                                           \
		if (((list)->head == SLIST_INVALID_OFFSET || (list)->tail == SLIST_INVALID_OFFSET) &&                  \
		    (list)->head != (list)->tail) {                                                                    \
			SLIST_INIT(runtime, list);                                                                     \
		} else if (SLIST_NEXT(type, runtime, (list)->tail, field) != SLIST_INVALID_OFFSET) {                   \
			store_with_flush(runtime, &(list)->tail, SLIST_NEXT(type, runtime, (list)->tail, field));      \
		}                                                                                                      \
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
	} while (0)

/* Invariants after crash:
 * If head was modified to point to 'node': list is consistent (node->next points to old head and tail is updated if
 * list was previously empty). If head was not modified: node->next might point to head, tail might point to new element
 * (it will be recovered on runtime init).
 */
#define SLIST_INSERT_HEAD(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		uint64_t *next = &SLIST_NEXT(type, runtime, offset, field);                                            \
		if ((list)->head == SLIST_INVALID_OFFSET) {                                                            \
			store_with_flush(runtime, &(list)->tail, offset);                                              \
		}                                                                                                      \
		store_with_flush(runtime, next, (list)->head);                                                         \
		(runtime)->drain();                                                                                    \
		store_with_flush(runtime, &(list)->head, offset);                                                      \
		(runtime)->drain();                                                                                    \
	} while (0)

/* Invariants after crash for non-empty list:
 * If tail was modified to point to 'node': list is consistent (node->next points to SLIST_INVALID_OFFSET, old tail
 * points to node) If tail was not modified: tail->next might point to node (it will be recovered on runtime init).
 */
#define SLIST_INSERT_TAIL(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		if ((list)->head == SLIST_INVALID_OFFSET) {                                                            \
			SLIST_INSERT_HEAD(type, runtime, list, offset, field);                                         \
		} else {                                                                                               \
			uint64_t *next = &SLIST_NEXT(type, runtime, offset, field);                                    \
			store_with_flush(runtime, next, SLIST_INVALID_OFFSET);                                         \
			(runtime)->drain();                                                                            \
			store_with_flush(runtime, &SLIST_NEXT(type, runtime, (list)->tail, field), offset);            \
			(runtime)->drain();                                                                            \
			store_with_flush(runtime, &(list)->tail, offset);                                              \
			(runtime)->drain();                                                                            \
		}                                                                                                      \
	} while (0)

#define SLIST_FOREACH(type, runtime, list, it, field)                                                                  \
	for ((it) = (list)->head; (it) != SLIST_INVALID_OFFSET; (it) = SLIST_NEXT(type, runtime, it, field))

#define SLIST_REMOVE_HEAD(type, runtime, list, field)                                                                  \
	do {                                                                                                           \
		if ((list)->head == SLIST_INVALID_OFFSET)                                                              \
			break;                                                                                         \
		if ((list)->tail == (list)->head) {                                                                    \
			SLIST_INIT(runtime, list);                                                                     \
		} else {                                                                                               \
			store_with_flush(runtime, &(list)->head, SLIST_NEXT(type, runtime, (list)->head, field));      \
			(runtime)->drain();                                                                            \
		}                                                                                                      \
	} while (0)

/* Invariants after crash for removing non-head 'node':
 * If node->next was modified: list is consistent (tail had been updated if necessary)
 * If node->next was not modified: tail might point element previous to last (it will be recovered on runtime init,
 * remove will be rolled back).
 */
#define SLIST_REMOVE(type, runtime, list, offset, field)                                                               \
	do {                                                                                                           \
		if ((list)->head == offset) {                                                                          \
			SLIST_REMOVE_HEAD(type, runtime, list, field);                                                 \
		} else {                                                                                               \
			uint64_t curelm = (list)->head;                                                                \
			while (SLIST_NEXT(type, runtime, curelm, field) != (offset))                                   \
				curelm = SLIST_NEXT(type, runtime, curelm, field);                                     \
			if (SLIST_NEXT(type, runtime, offset, field) == SLIST_INVALID_OFFSET) {                        \
				store_with_flush(runtime, &(list)->tail, offset);                                      \
				(runtime)->drain();                                                                    \
			}                                                                                              \
			store_with_flush(runtime, &SLIST_NEXT(type, runtime, curelm, field),                           \
					 SLIST_NEXT(type, runtime, offset, field));                                    \
			(runtime)->drain();                                                                            \
		}                                                                                                      \
	} while (0)

#ifdef __cplusplus
} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */
