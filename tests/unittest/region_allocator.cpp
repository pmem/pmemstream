// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.cpp -- tests region allocator */

#include "region_allocator.h"
#include "../examples/examples_helpers.h"
#include "libpmemstream.h"
#include "libpmemstream_internal.h"
#include "span.h"
#include "unittest.hpp"

#include <cstddef>
#include <iostream>

#include <rapidcheck.h>

#define DRAM

class test_allocator {
 public:
	test_allocator(uint64_t size)
	{
		area = new std::byte[size];
		region_allocator_new(&alloc_header, (uint64_t *)area, size);
	}
	size_t count_freelist()
	{
		return count_list(SPAN_EMPTY);
	}

	size_t count_alloclist()
	{
		return count_list(SPAN_REGION);
	}

	~test_allocator()
	{
		delete[] area;
	}

	struct allocator_header *get_alloc_header()
	{
		return alloc_header;
	}

 private:
	std::byte *area;
	struct allocator_header *alloc_header;

	size_t count_list(enum span_type type)
	{
		auto it = get_iterator_for_type(alloc_header, type);
		size_t i = 0;
		while (it != EMPTY_OBJ) {
			it = next(alloc_header, it);
			++i;
		}
		return i;
	}
};

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	uint64_t size = 16 * 1024;
#ifdef DRAM
	// uint64_t *spans = new uint64_t[size / sizeof(uint64_t)];
#else
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
	// uint64_t *spans = stream->data.spans;
#endif
	test_allocator alloc(size);
	struct allocator_header *alloc_header = alloc.get_alloc_header();

	return_check ret;
	ret += rc::check("Initialize new allocator", [alloc_header]() {
		RC_ASSERT(get_iterator_for_type(alloc_header, SPAN_REGION) == EMPTY_OBJ);
		RC_ASSERT(get_iterator_for_type(alloc_header, SPAN_EMPTY) != EMPTY_OBJ);
	});

	// auto res = split(alloc_header, get_iterator_for_type(alloc_header, SPAN_EMPTY), 512);
	// split(alloc_header, res, 256);

	// auto removed_elem = remove_head(alloc_header, &alloc_header->free_list);
	// push_back(alloc_header, &alloc_header->alloc_list, removed_elem);
	// set_next(alloc_header, removed_elem, EMPTY_OBJ);

	// removed_elem = remove_head(alloc_header, &alloc_header->free_list);
	// push_back(alloc_header, &alloc_header->alloc_list, removed_elem);
	// set_next(alloc_header, removed_elem, EMPTY_OBJ);

	// ret += rc::check("Split empty span", [res]() { RC_ASSERT(res != EMPTY_OBJ); });
	std::cout << "Allocation block on: " << region_allocator_allocate(alloc_header, 256, 64) << std::endl;
	std::cout << "Allocation block on: " << region_allocator_allocate(alloc_header, 500, 64) << std::endl;

	// ret += rc::check("is allocated-list empty",
	//		 [alloc_header]() { return (get_iterator_for_type(alloc_header, SPAN_REGION) == EMPTY_OBJ); });
	// ret += rc::check("is free-list not empty",
	//		 [alloc_header]() { return (get_iterator_for_type(alloc_header, SPAN_EMPTY) != EMPTY_OBJ); });
	/* iterate through free list and count items */

	std::cout << "Free list:" << std::endl;
	std::cout << "HEAD: " << alloc_header->free_list.head << std::endl;
	std::cout << "TAIL: " << alloc_header->free_list.tail << std::endl;
	auto it = get_iterator_for_type(alloc_header, SPAN_EMPTY);
	if (it == EMPTY_OBJ)
		std::cout << "EMPTY FREE LIST" << std::endl;
	while (it != EMPTY_OBJ) {
		std::cout << it << std::endl;
		it = next(alloc_header, it);
	}
	std::cout << "Alloc list:" << std::endl;
	std::cout << "HEAD: " << alloc_header->alloc_list.head << std::endl;
	std::cout << "TAIL: " << alloc_header->alloc_list.tail << std::endl;
	it = get_iterator_for_type(alloc_header, SPAN_REGION);
	if (it == EMPTY_OBJ)
		std::cout << "EMPTY ALLOC LIST" << std::endl;
	while (it != EMPTY_OBJ) {
		std::cout << it << std::endl;
		it = next(alloc_header, it);
	}

#ifdef DRAM

#else
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
#endif
	return 0;
}
