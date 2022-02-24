// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.cpp -- tests region allocator */

#include "region_allocator.h"
#include "../examples/examples_helpers.h"
#include "libpmemstream.h"
#include "libpmemstream_internal.h"
#include "span.h"
#include "unittest.hpp"

#include <iostream>

#include <rapidcheck.h>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	uint64_t size = 16 * 1024;
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
	struct allocator_header *alloc_header = nullptr;

	uint64_t *spans = stream->data.spans;
	region_allocator_new(&alloc_header, spans, size);

	return_check ret;
	ret += rc::check("is allocated-list empty",
			 [alloc_header]() { return (get_iterator_for_type(alloc_header, SPAN_REGION) == EMPTY_OBJ); });
	ret += rc::check("is free-list empty",
			 [alloc_header]() { return (get_iterator_for_type(alloc_header, SPAN_EMPTY) != EMPTY_OBJ); });
	(void)alloc_header;

	split(alloc_header, 2048);

	ret += rc::check("is allocated-list empty",
			 [alloc_header]() { return (get_iterator_for_type(alloc_header, SPAN_REGION) != EMPTY_OBJ); });
	ret += rc::check("is free-list empty",
			 [alloc_header]() { return (get_iterator_for_type(alloc_header, SPAN_EMPTY) != EMPTY_OBJ); });
	/* iterate through free list and count items */
	// split(spans, 512);
	/* iterate through free list */

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
	return 0;
}
