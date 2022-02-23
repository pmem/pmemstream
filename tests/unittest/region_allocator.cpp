// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.cpp -- tests region allocator */

#include "../examples/examples_helpers.h"
#include "libpmemstream.h"
#include "libpmemstream_internal.h"
#include "span.h"
#include "unittest.hpp"

#include <iostream>
#include <stdint.h>
#include <string.h>

#include <rapidcheck.h>

struct allocator_header {
	uint64_t magic;
	uint64_t first_free;	  // first empty span = 0
	uint64_t first_allocated; // first allocated region offset
	uint64_t data[];
} * header;
const uint64_t LIST_EMPTY = UINT64_MAX;

void initialize_memory(uint64_t *ptr, size_t size)
{
	header = (struct allocator_header *)ptr;
	header->first_free = 0;
	header->first_allocated = LIST_EMPTY;
	header->magic = 26985;

	span_empty se;
	se.span_base = span_base_create(size - sizeof(struct span_region), SPAN_EMPTY);
	memcpy(header->data, &se, sizeof(se));
}

uint64_t *pointer_to_offset(uint64_t *base, uint64_t offset)
{
	return (uint64_t *)((uint8_t *)base + offset);
}

int is_free_empty(uint64_t *ptr)
{
	if (span_get_type((span_base *)&ptr[0]) == SPAN_EMPTY) { // retrieve first element from list
		return 1;
	}
	return 0;
}

uint64_t split(uint64_t *ptr, size_t size)
{
	if (header->first_free == LIST_EMPTY) {
		return LIST_EMPTY;
	}
	uint64_t free_size = span_get_total_size((span_base *)pointer_to_offset(header->data, header->first_free));
	span_region sr{0};
	span_empty se{0};
	sr.span_base = span_base_create(size, SPAN_REGION);
	se.span_base = span_base_create(free_size - size, SPAN_EMPTY);
	printf("New size: %ld\n", size);
	printf("Free size: %ld\n", free_size);
	printf("Remaining size: %ld\n", free_size - size);
	memcpy(pointer_to_offset(header->data, header->first_free), &sr, sizeof(sr));	     // create region
	memcpy(pointer_to_offset(header->data, header->first_free + size), &se, sizeof(se)); // create empty space
	header->first_allocated = header->first_free;
	header->first_free += size; // set offset to first free

	return 0;
}

// move from one list to another
// region_allocator_initialize
// struct span_base span_base_create(uint64_t size, enum span_type type)

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	u_int64_t size = 16 * 1024;
	struct pmem2_map *map = example_map_open(argv[1], size);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	struct pmemstream *stream;
	if (pmemstream_from_map(&stream, 64, map) == -1) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return -1;
	}
	uint64_t *spans = stream->data.spans;
	initialize_memory(spans, size);

	return_check ret;
	ret += rc::check("is free empty", [spans]() {
		if (is_free_empty(spans) == 1) {
			return true;
		} else {
			return false;
		}
	});

	split(spans, 256);
	split(spans, 512);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
	return 0;
}