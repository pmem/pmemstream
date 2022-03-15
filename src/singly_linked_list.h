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

#define SLIST_FOREACH(var, runtime, list, type, field)                                                                 \
	for ((var) = (list)->head; (var) != UINT64_MAX;                                                                \
	     (var) = ((type *)(pmemstream_offset_to_ptr(runtime, var)))->field)

#ifdef __cplusplus

} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */