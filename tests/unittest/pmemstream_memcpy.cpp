#include "common/util.h"
#include "unittest.hpp"

#include <rapidcheck.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include <algorithm>
#include <memory>
#include <vector>

#include <iostream>

struct foo {
	size_t x;
	size_t y;
	size_t other_data[10];
	size_t padding[4];
	size_t aligned_data[8];
};

/* Test basic functionality of memcpy. pmemstream is needed only to get memcpy
 * implementation, as test copy data to dram structure */
static void test_basic(struct pmemstream *stream)
{
	struct foo *bar;
	size_t a = 2;
	size_t b = 1;
	size_t c[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	size_t d[] = {21, 22, 23, 24};
	size_t e[] = {31, 32, 33, 34, 35, 36, 37, 38};
	bar = (struct foo *)malloc(sizeof(struct foo));
	auto ret = pmemstream_memcpy(stream->memcpy, bar, &a, sizeof(a), &b, sizeof(b), c, sizeof(c), d, sizeof(d), e,
				     sizeof(e));

	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(bar->x, a);
	UT_ASSERTeq(bar->y, b);
	for (size_t i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
		UT_ASSERTeq(bar->other_data[i], c[i]);
	}
	for (size_t i = 0; i < sizeof(d) / sizeof(d[0]); i++) {
		UT_ASSERTeq(bar->padding[i], d[i]);
	}
	for (size_t i = 0; i < sizeof(e) / sizeof(e[0]); i++) {
		UT_ASSERTeq(bar->aligned_data[i], e[i]);
	}
	delete bar;
}

static void test_not_aligned_array(struct pmemstream *stream)
{
	size_t not_aligned_data[] = {0x1, 0x2};
	size_t buf[sizeof(not_aligned_data)];

	pmemstream_memcpy(stream->memcpy, buf, not_aligned_data, sizeof(not_aligned_data));
	for (size_t i = 0; i < sizeof(not_aligned_data) / sizeof(not_aligned_data[0]); i++) {
		UT_ASSERTeq(buf[i], not_aligned_data[i]);
	}
}

static auto property_tests(struct pmemstream *stream)
{
	return_check ret;
	ret += rc::check("verify if all data is copyied in correct order", [&](const std::vector<uint64_t> &data) {
		auto data_size = data.size() * sizeof(uint64_t);
		std::unique_ptr<uint64_t[]> output(new uint64_t[data.size()]);

		int ret_val = pmemstream_memcpy(stream->memcpy, output.get(), data.data(), data_size);
		RC_ASSERT(ret_val == 0);
		auto res = std::equal(data.begin(), data.end(), output.get());

		RC_ASSERT(res == true);
	});

	return ret;
}

int main(int argc, char *argv[])
{

	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}
	auto path = std::filesystem::path(argv[1]);

	if (!std::filesystem::exists(path)) {
		std::ofstream(path).put(0);
		std::filesystem::resize_file(path, 1024 * 1024);
	}

	struct pmem2_map *map = map_open(path.c_str(), std::filesystem::file_size(path));

	UT_ASSERTne(map, NULL);

	struct pmemstream *stream = NULL;
	pmemstream_from_map(&stream, 64, map);
	UT_ASSERTne(stream, NULL);

	// Add finaly labmda to run_test?
	return run_test([&] {
		test_basic(stream);
		test_not_aligned_array(stream);
		property_tests(stream);
	});

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}
