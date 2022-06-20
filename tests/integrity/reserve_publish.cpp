// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * reserve_publish.cpp -- pmemstream_reserve and pmemstream_publish integrity test.
 *			It checks specific cases of reserve-publish approach for writing data on pmem.
 */

#include <cstring>
#include <vector>

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);

	return run_test(test_config, [&] {
		return_check ret;

		/* this test leads to a "persistent" leak - do not try this at home! ;-) */
		ret += rc::check("verify if not calling publish does not result in data being visible",
				 [&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data,
				     const std::string &extra_entry) {
					 pmemstream_region region = stream.helpers.get_first_region();
					 stream.helpers.append(region, data);

					 auto [ret, reserved_entry, reserved_data] =
						 stream.sut.reserve(region, extra_entry.size());
					 UT_ASSERTeq(ret, 0);

					 std::memcpy(reinterpret_cast<char *>(reserved_data), extra_entry.data(),
						     extra_entry.size());

					 stream.helpers.verify(region, data, {});

					 stream.reopen();

					 stream.helpers.verify(region, data, {});
				 });
	});
}
