// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <cstdint>
#include <vector>

#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	auto path = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		/* 1. Allocate region and init it with data.
		 * 2. Verify that all data matches.
		 * 3. Append extra_data to the end.
		 * 4. Verify that all data matches.
		 */
		ret += rc::check("verify if iteration return proper elements after append",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data) {
					 pmemstream_sut stream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
					 auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE,
											       data);
					 stream.helpers.verify(region, data, {});
					 stream.helpers.append(region, extra_data);
					 stream.helpers.verify(region, data, extra_data);
				 });

		/* verify if an entry of size = 0 can be appended and entry with size > region's size cannot */
		{
			const size_t max_size = 1024UL;
			pmemstream_sut stream(path, max_size, TEST_DEFAULT_STREAM_SIZE);
			auto region = stream.helpers.initialize_single_region(max_size, {});
			stream.helpers.verify(region, {}, {});

			/* append an entry with size = 0 */
			std::string entry;
			auto [ret, new_entry] = stream.append(region, entry);
			UT_ASSERTeq(ret, 0);
			stream.helpers.verify(region, {entry}, {});

			/* and try to append entry with size bigger than region's size */
			entry = std::string(max_size * 2, 'W');
			std::tie(ret, new_entry) = stream.append(region, entry);
			UT_ASSERTeq(ret, -1);
		}

		ret += rc::check("verify if appending entry of size = 0 and invalid address do not cause segfault",
				 [&](const uintptr_t &invalid_data_address) {
					 pmemstream_sut stream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
					 auto region =
						 stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});
					 stream.helpers.verify(region, {}, {});

					 /* append an entry with size = 0 and invalid address */
					 std::string entry;

					 auto invalid_data_ptr = reinterpret_cast<char *>(invalid_data_address);
					 auto [ret, new_entry] =
						 stream.append(region, std::string_view(invalid_data_ptr, 0));
					 UT_ASSERTeq(ret, 0);
					 stream.helpers.verify(region, {entry}, {});
				 });

		ret += rc::check("verify append will work until OOM", [&]() {
			pmemstream_sut stream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
			auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});

			size_t elems = 10;
			const size_t e_size = TEST_DEFAULT_REGION_SIZE / elems - TEST_DEFAULT_BLOCK_SIZE;
			std::string e = *rc::gen::container<std::string>(e_size, rc::gen::character<char>());

			struct pmemstream_entry prev_ne = {0};
			while (elems-- > 0) {
				auto [ret, new_entry] = stream.append(region, e);
				UT_ASSERTeq(ret, 0);
				UT_ASSERT(new_entry.offset > prev_ne.offset);
				prev_ne = new_entry;
			}
			/* next append should not fit */
			auto [ret, new_entry] = stream.append(region, e);
			UT_ASSERTeq(new_entry.offset, prev_ne.offset);
			/* XXX: should be updated with the real error code, when available */
			UT_ASSERTeq(ret, -1);
			e.resize(4);
			/* ... but smaller entry should fit just in */
			std::tie(ret, new_entry) = stream.append(region, e);
			UT_ASSERT(new_entry.offset > prev_ne.offset);
			UT_ASSERTeq(ret, 0);

			UT_ASSERTeq(stream.region_free(region), 0);
		});

		ret += rc::check("verify if iteration return proper elements after pmemstream reopen",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data) {
					 pmemstream_sut stream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
					 pmemstream_region region = stream.helpers.initialize_single_region(
						 TEST_DEFAULT_REGION_SIZE, data);
					 stream.helpers.verify(region, data, {});
					 stream.reopen();
					 stream.helpers.verify(region, data, {});
					 UT_ASSERTeq(stream.region_free(region), 0);
				 });

		ret += rc::check("verify if iteration return proper elements after append after pmemstream reopen",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
				     bool user_created_runtime) {
					 pmemstream_sut stream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
					 pmemstream_region region = stream.helpers.initialize_single_region(
						 TEST_DEFAULT_REGION_SIZE, data);
					 stream.helpers.verify(region, data, {});
					 stream.reopen();

					 if (user_created_runtime) {
						 stream.region_runtime_initialize(region);
					 }

					 stream.helpers.append(region, extra_data);
					 stream.helpers.verify(region, data, extra_data);
					 UT_ASSERTeq(stream.region_free(region), 0);
				 });
	});
}
