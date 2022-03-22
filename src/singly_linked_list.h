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
			while (SLIST_NEXT(type, runtime, curelm, field) != (offset))                                   \
				curelm = SLIST_NEXT(type, runtime, curelm, field);                                     \
			if ((SLIST_NEXT(type, runtime, curelm, field) = SLIST_NEXT(type, runtime, offset, field)) ==   \
			    SLIST_INVALID_OFFSET)                                                                      \
				(list)->tail = offset;                                                                 \
		}                                                                                                      \
	} while (0)

#ifdef __cplusplus
} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */
