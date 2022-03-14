// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_SINGLY_LINKED_LIST_H
#define LIBPMEMSTREAM_SINGLY_LINKED_LIST_H

#include "stdint.h"

#include "libpmemstream_internal.h"

struct singly_linked_list {
	uint64_t head;
	uint64_t tail;
};

#ifdef __cplusplus
extern "C" {
#endif

// pmemstream_data_runtime
void push_front(struct pmemstream_data_runtime *data, struct singly_linked_list *list, uint64_t offset);
void push_back(struct pmemstream_data_runtime *data, struct singly_linked_list *list, uint64_t offset);
void remove_head(struct pmemstream_data_runtime *data, struct singly_linked_list *list);
uint64_t next(struct pmemstream_data_runtime *data, uint64_t it);
#ifdef __cplusplus

} /* end extern "C */
#endif

#endif /* LIBPMEMSTREAM_SINGLY_LINKED_LIST_H */