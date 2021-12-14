// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "libpmemstream_internal.h"
#include "unittest.hpp"

#include <rapidcheck.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

struct aligned_struct {
	size_t x;
	size_t y;
	size_t other_data[10];
	size_t padding[4];
	size_t aligned_data[8];
};

/* Test basic functionality of memcpy with cache line aligned data.
 * pmemstream is needed only to get memcpy implementation, as the test copy data to the dram structure */
static void test_basic(struct pmemstream *stream)
{
	auto to_fill = aligned_struct();
	size_t a = 2;
	size_t b = 1;
	size_t c[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	size_t d[] = {21, 22, 23, 24};
	size_t e[] = {31, 32, 33, 34, 35, 36, 37, 38};
	auto ret = pmemstream_memcpy(stream->memcpy, &to_fill, &a, sizeof(a), &b, sizeof(b), c, sizeof(c), d, sizeof(d),
				     e, sizeof(e));

	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(to_fill.x, a);
	UT_ASSERTeq(to_fill.y, b);
	for (size_t i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
		UT_ASSERTeq(to_fill.other_data[i], c[i]);
	}
	for (size_t i = 0; i < sizeof(d) / sizeof(d[0]); i++) {
		UT_ASSERTeq(to_fill.padding[i], d[i]);
	}
	for (size_t i = 0; i < sizeof(e) / sizeof(e[0]); i++) {
		UT_ASSERTeq(to_fill.aligned_data[i], e[i]);
	}
}

/* Test memcpy with data not aligned to the cache line */
static void test_not_aligned_array(struct pmemstream *stream)
{
	size_t not_aligned_data[] = {0x1, 0x2};
	size_t buf[sizeof(not_aligned_data) / sizeof(*not_aligned_data)];

	pmemstream_memcpy(stream->memcpy, buf, not_aligned_data, sizeof(not_aligned_data));
	for (size_t i = 0; i < sizeof(not_aligned_data) / sizeof(not_aligned_data[0]); i++) {
		UT_ASSERTeq(buf[i], not_aligned_data[i]);
	}
}

int main(int argc, char *argv[])
{
	/* XXX: Use consistent approach for file creation across all tests.
	 * Probably this is the one which we want to use. */
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}
	auto path = std::filesystem::path(argv[1]);

	if (!std::filesystem::exists(path)) {
		std::ofstream(path).put(0);
		std::filesystem::resize_file(path, 1024 * 1024);
	}
	static constexpr size_t block_size = 4096;
	auto stream = make_pmemstream(path, block_size, std::filesystem::file_size(path));

	return run_test([&] {
		test_basic(stream.get());
		test_not_aligned_array(stream.get());
	});
}
