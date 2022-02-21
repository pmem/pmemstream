// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * reserve_publish.cpp -- pmemstream_reserve and pmemstream_publish functional test.
 *			It checks if we can reserve space for entry, write to that space, and persist it.
 *			It's executed among "regular" appends to confirm we can mix these up.
 */

#include <cstring>
#include <vector>

#include <rapidcheck.h>

#include "libpmemstream_internal.h"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		UT_FATAL("usage: %s file-path", argv[0]);
	}

	auto path = std::string(argv[1]);

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.stream_size = TEST_DEFAULT_STREAM_SIZE;
	test_config.block_size = TEST_DEFAULT_BLOCK_SIZE;

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check("verify if mixing reserve+publish with append works fine",
				 [&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, const bool use_append) {
					 auto region = stream.helpers.get_first_region();
					 stream.helpers.append(region, data);

					 if (use_append) {
						 stream.helpers.append(region, extra_data);
					 } else {
						 stream.helpers.reserve_and_publish(region, extra_data);
					 }

					 /* add one more "regular" append */
					 std::vector<std::string> my_data(extra_data);
					 my_data.emplace_back(1024, 'Z');
					 const auto extra_entry = my_data.back();
					 auto [ret, new_entry] = stream.sut.append(region, extra_entry);
					 UT_ASSERTeq(ret, 0);
					 stream.helpers.verify(region, data, my_data);

					 UT_ASSERTeq(stream.sut.region_free(region), 0);
				 });

		ret += rc::check("verify if reserve+publish by hand will behave the same as regular append",
				 [&](const std::vector<std::string> &data, const bool is_runtime_initialized) {
					 /* regular append of 'data' */
					 std::vector<std::string> a_data;
					 {
						 pmemstream_test_base stream(path, get_test_config().block_size,
									     get_test_config().stream_size);
						 auto region = stream.helpers.initialize_single_region(
							 TEST_DEFAULT_REGION_SIZE, data);
						 stream.helpers.verify(region, data, {});
						 a_data = stream.helpers.get_elements_in_region(region);

						 UT_ASSERTeq(stream.sut.region_free(region), 0);
					 }
					 /* publish-reserve by hand of the same 'data' (in a different file) */
					 std::vector<std::string> rp_data;
					 {
						 pmemstream_test_base stream(path + "_2", get_test_config().block_size,
									     get_test_config().stream_size);
						 auto region = stream.helpers.initialize_single_region(
							 TEST_DEFAULT_REGION_SIZE, {});
						 stream.helpers.reserve_and_publish(region, data);
						 rp_data = stream.helpers.get_elements_in_region(region);

						 UT_ASSERT(std::equal(a_data.begin(), a_data.end(), rp_data.begin(),
								      rp_data.end()));

						 UT_ASSERTeq(stream.sut.region_free(region), 0);
					 }
				 });

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
