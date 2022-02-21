// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <cstdint>
#include <vector>

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

		ret += rc::check("verify if iteration return proper elements after append",
				 [&](pmemstream_empty &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, bool reopen) {
					 auto region = stream.sut.helpers.initialize_single_region(
						 TEST_DEFAULT_REGION_SIZE, data);

					 if (reopen)
						 stream.sut.reopen();

					 stream.sut.helpers.verify(region, data, {});
					 stream.sut.helpers.append(region, extra_data);
					 stream.sut.helpers.verify(region, data, extra_data);
				 });

		/* Verify if empty region does not return any data. */
		{
			pmemstream_with_single_empty_region stream;
			stream.sut.helpers.verify(stream.sut.helpers.get_first_region(), {}, {});
		}

		/* verify if an entry of size = 0 can be appended */
		{
			pmemstream_with_single_empty_region stream;
			auto region = stream.sut.helpers.get_first_region();

			std::string entry;
			auto [ret, new_entry] = stream.sut.append(region, entry);
			UT_ASSERTeq(ret, 0);
			UT_ASSERT(entry == stream.sut.get_entry(new_entry));
			stream.sut.helpers.verify(region, {entry}, {});
		}

		/* and entry with size > region's size cannot be appended */
		{
			pmemstream_with_single_empty_region stream;
			auto region = stream.sut.helpers.get_first_region();
			auto region_size = stream.sut.region_size(region);

			auto entry = std::string(region_size * 2, 'W');
			auto [ret, new_entry] = stream.sut.append(region, entry);
			UT_ASSERTeq(ret, -1);
		}

		ret += rc::check(
			"verify if appending entry of size = 0 and invalid address do not cause segfault",
			[&](pmemstream_with_single_empty_region &&stream, const uintptr_t &invalid_data_address) {
				auto region = stream.sut.helpers.get_first_region();

				/* append an entry with size = 0 and invalid address */
				std::string entry;
				auto invalid_data_ptr = reinterpret_cast<char *>(invalid_data_address);
				auto [ret, new_entry] =
					stream.sut.append(region, std::string_view(invalid_data_ptr, 0));
				UT_ASSERTeq(ret, 0);
				UT_ASSERT(entry == stream.sut.get_entry(new_entry));
				stream.sut.helpers.verify(region, {entry}, {});
			});

		ret += rc::check("verify if sequence of append and reopen commands leads to consitent state", [] {
			pmemstream_model model;
			pmemstream_with_single_empty_region s;

			model.is_runtime_initialized = *rc::gen::arbitrary<bool>();
			model.is_runtime_initialized_after_reopen = *rc::gen::arbitrary<bool>();
			model.regions[s.sut.helpers.get_first_region().offset] = {};

			rc::state::check(model, s.sut,
					 rc::state::gen::execOneOfWithArgs<rc_append_command, rc_reopen_command,
									   rc_verify_command>());
		});
	});
}
