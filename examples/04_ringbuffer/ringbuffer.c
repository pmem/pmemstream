// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "ringbuffer.h"
#include <string.h>

static struct ringbuffer_possition ringbuffer_possition_begin(struct pmemstream *stream) {
	struct pmemstream_region_iterator *region_iterator = NULL;

        pmemstream_region_iterator_new(&region_iterator, stream);

	struct pmemstream_region *first_region = NULL;
	struct pmemstream_entry *first_entry = NULL;

	pmemstream_region_iterator_next(region_iterator, first_region);
	struct pmemstream_entry_iterator *entry_iterator = NULL;
	pmemstream_entry_iterator_new(&entry_iterator, stream, *first_region);
	pmemstream_entry_iterator_next(entry_iterator, first_region, first_entry);

	struct ringbuffer_possition new_possition = { .stream = stream,
		.region_iterator = region_iterator,
		.current_region = first_region,
		.entry_iterator = entry_iterator,
		.current_entry = first_entry};
	return new_possition;
};

//XXX: handle errors
static int ringbuffer_possition_next(struct ringbuffer_possition *position){
	struct pmemstream_entry *entry=NULL;
	struct pmemstream_region *region;

	int entry_ret = pmemstream_entry_iterator_next(position->entry_iterator, NULL, entry);
	if(entry_ret != 0) {
		int region_ret = pmemstream_region_iterator_next(position->region_iterator, region);
		if( region_ret != 0) {
			return region_ret;
		}
		pmemstream_entry_iterator_delete(&position->entry_iterator);
		pmemstream_entry_iterator_new(&position->entry_iterator, position->stream, *region);
	}
	return 0;
}

static bool ringbuffer_possition_equal(struct ringbuffer_possition *lhs, struct ringbuffer_possition *rhs) {
	return pmemstream_entry_iterator_equal(lhs->entry_iterator, rhs->entry_iterator);
}

static void ringbuffer_possition_delete(struct ringbuffer_possition *position) {
	pmemstream_region_iterator_delete(&position->region_iterator);
	pmemstream_entry_iterator_delete(&position->entry_iterator);
}

/* Do it in separate worker thread */
static int ringbuffer_runtime_reallocate_region(struct ringbuffer_runtime *runtime) {
	struct pmemstream_region_iterator *it;
	pmemstream_region_iterator_new( &it, runtime->stream);

	struct pmemstream_region region;
	pmemstream_region_iterator_next(it, &region);

	if(pmemstream_region_iterator_equal(runtime->consumer_position.region_iterator, it)) {
		return -1;
	} else {
		pmemstream_region_free(runtime->stream, region);
		int ret = pmemstream_region_allocate(runtime->stream, runtime->region_size, NULL);
		if( ret != 0){
			return ret;
		}
	}

	pmemstream_region_iterator_delete(&it);
	return 0;
}

struct ringbuffer_runtime ringbuffer_runtime_new(struct pmemstream *stream, size_t region_size) {
	struct ringbuffer_possition begin = ringbuffer_possition_begin(stream);
	struct ringbuffer_runtime runtime = {.stream = stream,
		.region_size = region_size,
		.producer_position = begin,
		.consumer_position = begin};
	return runtime;
}

size_t ringbuffer_runtime_consume(struct ringbuffer_runtime *runtime, void *data) {
	/* Ringbuffer if empty*/
	if(ringbuffer_possition_equal(&runtime->producer_position, &runtime->consumer_position)){
		return 0;
	}
	struct pmemstream_entry *entry = runtime->consumer_position.current_entry;

	const void *entry_data = pmemstream_entry_data(runtime->stream, *entry);
	const size_t entry_length = pmemstream_entry_length(runtime->stream, *entry);

	memcpy(data, entry_data, entry_length);

	ringbuffer_possition_next(&runtime->consumer_position);

	return entry_length;
}

/* Copy data to ringbuffer */
bool ringbuffer_runtime_produce(struct ringbuffer_runtime *runtime, const void *data, size_t size){
	struct ringbuffer_possition *current_position = &(runtime->producer_position);
	struct ringbuffer_possition next_possition = runtime->producer_position;
	if(ringbuffer_possition_next(&next_possition) == 0)  {
		ringbuffer_runtime_reallocate_region(runtime);
		return false;
	}

	if(pmemstream_append(runtime->stream, *current_position->current_region, NULL, data, size, NULL)){
		return false;
	}

	return true;
}

void ringbuffer_runtime_delete(struct ringbuffer_runtime *runtime){
	ringbuffer_possition_delete(&runtime->producer_position);
	ringbuffer_possition_delete(&runtime->consumer_position);
}

