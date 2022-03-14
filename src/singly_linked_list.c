// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "singly_linked_list.h"

const uint64_t EMPTY_OBJ = UINT64_MAX;

struct pmemstream_data_runtime_mock {
	uint64_t *spans;

};

#define CHECK_MOCK_FIELDS(structure) do \
{ \
	assert(structure.spans); \
	assert(structure.memcpy); \
	assert(structure.memset); \
	assert(structure.flush); \
	assert(structure.drain); \
	assert(structure.persist); \
} while(/*COND*/0);

void push_front(struct pmemstream_data_runtime *data, struct singly_linked_list *list, uint64_t offset)
{
	if (list->head == list->tail) {
		list->tail = offset;
	}
	set_next(data, offset, list->head);
	list->head = offset;
}

void push_back(struct pmemstream_data_runtime *data, struct singly_linked_list *list, uint64_t offset)
{
	if (list->head == EMPTY_OBJ && list->tail == EMPTY_OBJ) {
		list->head = offset;
		list->tail = offset;
		set_next(data, list->tail, EMPTY_OBJ);
		return;
	}
	set_next(data, list->tail, offset);
	list->tail = offset;
}

void remove_head(struct pmemstream_data_runtime *data, struct singly_linked_list *list)
{
	if (list->head == EMPTY_OBJ)
		return EMPTY_OBJ;
	uint64_t to_be_deleted = list->head;
	list->head = next(data, list->head);
	return to_be_deleted;
}
uint64_t next(struct pmemstream_data_runtime *data, uint64_t it)
{
	return UINT64_MAX;
}
