// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * Source: glibc 2.24 (git://sourceware.org/glibc.git /misc/sys/queue.h)
 *
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)queue.h     8.5 (Berkeley) 8/20/94
 */

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

static inline void store_with_flush(const struct pmemstream_runtime *runtime, uint64_t *dst, uint64_t value)
{
	__atomic_store_n(dst, value, __ATOMIC_RELAXED);
	(runtime)->flush(dst, sizeof(*dst));
}

#define SLIST_INVARIANT_HEAD(type, runtime, list, field)                                                               \
	(((list)->head != SLIST_INVALID_OFFSET) ||                                                                     \
	 ((list)->head == SLIST_INVALID_OFFSET && (list)->tail == SLIST_INVALID_OFFSET))
#define SLIST_INVARIANT_TAIL(type, runtime, list, field)                                                               \
	(((list)->tail != SLIST_INVALID_OFFSET) ||                                                                     \
	 ((list)->head == SLIST_INVALID_OFFSET && (list)->tail == SLIST_INVALID_OFFSET))
#define SLIST_INVARIANT_TAIL_NEXT(type, runtime, list, field)                                                          \
	(((list)->tail == SLIST_INVALID_OFFSET) ||                                                                     \
	 (SLIST_NEXT(type, runtime, (list)->tail, field) == SLIST_INVALID_OFFSET))
#define SLIST_INVARIANTS(type, runtime, list, field)                                                                   \
	(SLIST_INVARIANT_HEAD(type, runtime, list, field) && SLIST_INVARIANT_TAIL(type, runtime, list, field) &&       \
	 SLIST_INVARIANT_TAIL_NEXT(type, runtime, list, field))

#define SLIST_INIT(runtime, list)                                                                                      \
	do {                                                                                                           \
		store_with_flush(runtime, &(list)->tail, SLIST_INVALID_OFFSET);                                        \
		store_with_flush(runtime, &(list)->head, SLIST_INVALID_OFFSET);                                        \
		(runtime)->drain();                                                                                    \
	} while (0)

/* Recovers the list after restart - ensures that all invariants are true. */
#define SLIST_RUNTIME_INIT(type, runtime, list, field)                                                                 \
	do {                                                                                                           \
		if (((list)->head == SLIST_INVALID_OFFSET || (list)->tail == SLIST_INVALID_OFFSET) &&                  \
		    (list)->head != (list)->tail) {                                                                    \
			SLIST_INIT(runtime, list);                                                                     \
		} else if ((list)->head != SLIST_INVALID_OFFSET &&                                                     \
			   SLIST_NEXT(type, runtime, (list)->tail, field) != SLIST_INVALID_OFFSET) {                   \
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
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
		uint64_t *next = &SLIST_NEXT(type, runtime, offset, field);                                            \
		if ((list)->head == SLIST_INVALID_OFFSET) {                                                            \
			store_with_flush(runtime, &(list)->tail, offset);                                              \
		}                                                                                                      \
		store_with_flush(runtime, next, (list)->head);                                                         \
		(runtime)->drain();                                                                                    \
		store_with_flush(runtime, &(list)->head, offset);                                                      \
		(runtime)->drain();                                                                                    \
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
	} while (0)

/* Invariants after crash for non-empty list:
 * If tail was modified to point to 'node': list is consistent (node->next points to SLIST_INVALID_OFFSET, old tail
 * points to node) If tail was not modified: tail->next might point to node (it will be recovered on runtime init).
 */
#define SLIST_INSERT_TAIL(type, runtime, list, offset, field)                                                          \
	do {                                                                                                           \
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
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
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
	} while (0)

#define SLIST_FOREACH(type, runtime, list, it, field)                                                                  \
	for ((it) = (list)->head; (it) != SLIST_INVALID_OFFSET; (it) = SLIST_NEXT(type, runtime, it, field))

/* Invariants after crash:
 * If list has only one element: tail might be different from head (it will be recovered on runtime init).
 * If list has more than one element: we perform only a single atomic store - list is consistent.
 */
#define SLIST_REMOVE_HEAD(type, runtime, list, field)                                                                  \
	do {                                                                                                           \
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
		if ((list)->head == SLIST_INVALID_OFFSET)                                                              \
			break;                                                                                         \
		if ((list)->tail == (list)->head) {                                                                    \
			SLIST_INIT(runtime, list);                                                                     \
		} else {                                                                                               \
			store_with_flush(runtime, &(list)->head, SLIST_NEXT(type, runtime, (list)->head, field));      \
			(runtime)->drain();                                                                            \
		}                                                                                                      \
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
	} while (0)

/* Invariants after crash for removing non-head 'node':
 * If node->next was modified: list is consistent (tail had been updated if necessary)
 * If node->next was not modified: tail might point to element previous to last (it will be recovered on runtime init,
 * remove will be rolled back).
 */
#define SLIST_REMOVE(type, runtime, list, offset, field)                                                               \
	do {                                                                                                           \
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
		if ((list)->head == offset) {                                                                          \
			SLIST_REMOVE_HEAD(type, runtime, list, field);                                                 \
		} else {                                                                                               \
			uint64_t curelm = (list)->head;                                                                \
			while (SLIST_NEXT(type, runtime, curelm, field) != offset)                                     \
				curelm = SLIST_NEXT(type, runtime, curelm, field);                                     \
			if (SLIST_NEXT(type, runtime, offset, field) == SLIST_INVALID_OFFSET) {                        \
				store_with_flush(runtime, &(list)->tail, curelm);                                      \
				(runtime)->drain();                                                                    \
			}                                                                                              \
			store_with_flush(runtime, &SLIST_NEXT(type, runtime, curelm, field),                           \
					 SLIST_NEXT(type, runtime, offset, field));                                    \
			(runtime)->drain();                                                                            \
		}                                                                                                      \
		assert(SLIST_INVARIANTS(type, runtime, list, field));                                                  \
	} while (0)

#ifdef __cplusplus
} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */
