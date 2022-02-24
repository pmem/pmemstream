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
	uint64_t *spans = stream->data.spans;
	initialize_memory(spans, size);

	return_check ret;
	ret += rc::check("is free empty", [spans]() {
		// if (is_free_empty(spans) == true) {
		// 	return true;
		// } else {
		// 	return false;
		// }
	});

	// split(spans, 2048);
	/* iterate through free list */
	// split(spans, 512);
	/* iterate through free list */

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
	return 0;
}
