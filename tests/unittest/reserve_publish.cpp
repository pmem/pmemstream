// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * reserve_publish.cpp -- pmemstream_reserve and pmemstream_publish functional test.
 *			It checks if we can reserve space for entry, write to that space, and persist it.
 *			It's executed after or before "regular" appends to confirm we can mix these up.
 */

#include <cstring>
#include <vector>

#include <rapidcheck.h>

#include "libpmemstream_internal.h"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

namespace
{
std::tuple<std::vector<std::string>, size_t> verify_appended_data(struct pmemstream_test_base &stream,
								  struct pmemstream_region region,
								  const std::vector<std::string> &expected_data)
{
	auto data = stream.helpers.get_elements_in_region(region);
	stream.helpers.verify(region, data, {});
	uint64_t p_tmstp = stream.sut.persisted_timestamp();
	uint64_t c_tmstp = stream.sut.committed_timestamp();
	UT_ASSERTeq(c_tmstp, p_tmstp);
	return {data, p_tmstp};
}
} /* namespace */

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

		ret += rc::check("verify if mixing reserve+publish with append works fine",
				 [&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, bool verify_in_middle) {
					 auto region = stream.helpers.get_first_region();

					 stream.helpers.append(region, data);
					 if (verify_in_middle) {
						 stream.helpers.verify(region, data, {});
					 }

					 stream.helpers.reserve_and_publish(region, extra_data);
					 stream.helpers.verify(region, data, extra_data);
				 });

		ret += rc::check("verify if mixing reserve+publish with append works fine",
				 [&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, const bool use_append,
				     std::string extra_append) {
					 auto region = stream.helpers.get_first_region();
					 stream.helpers.append(region, data);

					 stream.helpers.reserve_and_publish(region, extra_data);

					 std::vector<std::string> all_extra_data(extra_data);

					 /* add one more "regular" append */
					 if (use_append) {
						 all_extra_data.emplace_back(extra_append);
						 auto [ret, new_entry] = stream.sut.append(region, extra_append);
						 UT_ASSERTeq(ret, 0);
					 }

					 stream.helpers.verify(region, data, all_extra_data);

					 UT_ASSERTeq(stream.sut.region_free(region), 0);
				 });

		ret += rc::check(
			"reserve+publish by hand will behave the same as regular append in 2 stream instances",
			[&](const std::vector<std::string> &data) {
				/* regular append of 'data' */
				std::vector<std::string> a_data;
				uint64_t a_timestamp = PMEMSTREAM_INVALID_OFFSET;
				{
					pmemstream_test_base stream(get_test_config().filename,
								    get_test_config().block_size,
								    get_test_config().stream_size);
					auto region =
						stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});
					stream.helpers.append(region, data);

					auto [ret_data, ret_tmstp] = verify_appended_data(stream, region, data);
					a_data = ret_data;
					a_timestamp = ret_tmstp;
				}
				/* publish-reserve by hand of the same 'data' (in a different file) */
				std::vector<std::string> rp_data;
				uint64_t rp_timestamp = PMEMSTREAM_INVALID_OFFSET;
				{
					pmemstream_test_base stream(get_test_config().filename + "_2",
								    get_test_config().block_size,
								    get_test_config().stream_size);
					auto region =
						stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});
					stream.helpers.reserve_and_publish(region, data);

					auto [ret_data, ret_tmstp] = verify_appended_data(stream, region, data);
					rp_data = ret_data;
					rp_timestamp = ret_tmstp;
				}

				UT_ASSERT(std::equal(a_data.begin(), a_data.end(), rp_data.begin(), rp_data.end()));
				UT_ASSERTeq(a_timestamp, rp_timestamp);
			});

		test_config.region_size = TEST_DEFAULT_REGION_MULTI_SIZE;
		ret += rc::check(
			"reserve+publish by hand will behave the same as regular append in 2 separate regions",
			[&](pmemstream_empty &&stream, const std::vector<std::string> &data) {
				/* regular append of 'data' */
				auto r1 = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_MULTI_SIZE, {});
				stream.helpers.append(r1, data);
				auto [a_data, a_timestamp] = verify_appended_data(stream, r1, data);

				/* publish-reserve by hand of the same 'data' (in a different region) */
				auto r2 = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_MULTI_SIZE, {});
				stream.helpers.reserve_and_publish(r2, data);
				auto [rp_data, rp_timestamp] = verify_appended_data(stream, r2, data);

				UT_ASSERT(std::equal(a_data.begin(), a_data.end(), rp_data.begin(), rp_data.end()));
				UT_ASSERTeq(a_timestamp, data.size());
				UT_ASSERTeq(a_timestamp * 2, rp_timestamp);
			});
	});
}
