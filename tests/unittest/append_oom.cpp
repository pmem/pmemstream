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

		ret += rc::check(
			"verify append will work until OOM", [&](pmemstream_with_single_empty_region &&stream) {
				auto region = stream.sut.helpers.get_first_region();

				size_t elems = 10;
				const size_t e_size = TEST_DEFAULT_REGION_SIZE / elems - TEST_DEFAULT_BLOCK_SIZE;
				std::string e = *rc::gen::container<std::string>(e_size, rc::gen::character<char>());

				struct pmemstream_entry prev_ne = {0};
				while (elems-- > 0) {
					auto [ret, new_entry] = stream.sut.append(region, e);
					UT_ASSERTeq(ret, 0);
					UT_ASSERT(new_entry.offset > prev_ne.offset);
					prev_ne = new_entry;
				}
				/* next append should not fit */
				auto [ret, new_entry] = stream.sut.append(region, e);
				UT_ASSERTeq(new_entry.offset, prev_ne.offset);
				/* XXX: should be updated with the real error code, when available */
				UT_ASSERTeq(ret, -1);
				e.resize(4);
				/* ... but smaller entry should fit just in */
				std::tie(ret, new_entry) = stream.sut.append(region, e);
				UT_ASSERT(new_entry.offset > prev_ne.offset);
				UT_ASSERTeq(ret, 0);

				UT_ASSERTeq(stream.sut.region_free(region), 0);
			});
	});
}
