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

const uint64_t EMPTY_OBJ = UINT64_MAX;

struct allocator_header {
	uint64_t magic;
	uint64_t first_free;	  // first empty span = 0
	uint64_t first_allocated; // first allocated region offset
	uint64_t padding[5];
	uint64_t data[];
} * header;

/* API */
uint64_t allocate_region(uint64_t region_size);
void free_region(uint64_t region_offset);
uint64_t get_region_iterator();
uint64_t get_empty_iterator();
uint64_t next(uint64_t it);

void initialize_memory(uint64_t *ptr, size_t size)
{
	const uint64_t header_id = 26985;
	header = (struct allocator_header *)ptr;
	if (header->magic == header_id)
		return;

	header->first_free = 0;
	header->first_allocated = EMPTY_OBJ;
	header->magic = header_id;

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
	if (header->first_free == EMPTY_OBJ) {
		return EMPTY_OBJ;
	}
	uint64_t free_size = span_get_total_size((span_base *)pointer_to_offset(header->data, header->first_free));
	if (size > free_size + sizeof(struct span_empty)) {
		return EMPTY_OBJ;
	}
	span_empty se[2] = {0};
	size_t target_size[2] = {size, free_size - size}; // size plus metadata size (for empty or region?)
	uint64_t target_offset[2] = {header->first_free, header->first_free + size};
	se[0].span_base = span_base_create(target_size[0], SPAN_EMPTY);
	se[1].span_base = span_base_create(target_size[1], SPAN_EMPTY);
	se[0].next = target_offset[1];
	memcpy(pointer_to_offset(header->data, target_offset[0]), &se[0], sizeof(span_empty));
	memcpy(pointer_to_offset(header->data, target_offset[1]), &se[1], sizeof(span_empty));
	return 0;
}

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
		if (is_free_empty(spans) == true) {
			return true;
		} else {
			return false;
		}
	});

	split(spans, 2048);
	/* iterate through free list */
	split(spans, 512);
	/* iterate through free list */

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
	return 0;
}
