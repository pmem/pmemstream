// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "common/util.h"
#include "unittest.hpp"

#include <rapidcheck.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

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

	static constexpr size_t block_size = 4096;
	auto stream = make_pmemstream(path, block_size, std::filesystem::file_size(path));

	return run_test([&] {
		return_check ret;

		ret += rc::check(
			"verify if all data is copied in correct order", [&](const std::vector<uint64_t> &data) {
				auto data_size = data.size() * sizeof(uint64_t);
				std::unique_ptr<uint64_t[]> output(new uint64_t[data.size()]);

				int ret_val = pmemstream_memcpy(stream->memcpy, output.get(), data.data(), data_size);
				RC_ASSERT(ret_val == 0);
				auto res = std::equal(data.begin(), data.end(), output.get());

				RC_ASSERT(res);
			});
	});
}
