// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include "libpmemstream.h"
#include <stdbool.h>

struct ringbuffer_possition {
	struct pmemstream *stream;
	struct pmemstream_region_iterator *region_iterator;
	//XXX: Add pmemstream_region_iterator_get()
	struct pmemstream_region *current_region;

	struct pmemstream_entry_iterator *entry_iterator;
	//XXX: Add pmemstream_region_iterator_get()
	struct pmemstream_entry *current_entry;
};

struct ringbuffer_runtime {
	struct pmemstream *stream;
	size_t region_size;

	struct ringbuffer_possition producer_position;
	struct ringbuffer_possition consumer_position;
};

//XXX: should it return ptr?
struct ringbuffer_runtime ringbuffer_runtime_new(struct pmemstream *stream, size_t region_size);

bool ringbuffer_runtime_produce(struct ringbuffer_runtime *runtime, const void *data, size_t size);

size_t ringbuffer_runtime_consume(struct ringbuffer_runtime *runtime, void *data);

void ringbuffer_runtime_delete(struct ringbuffer_runtime *runtime);

#endif /* RINGBUFFER_H */


