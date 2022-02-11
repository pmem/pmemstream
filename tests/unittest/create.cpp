// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* create.cpp -- tests creation of regions and streams with various parameters */

#include <array>
#include <string>
#include <vector>

#include <rapidcheck.h>

#include "common/util.h"
#include "span.h"
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

		ret += rc::check("verify if a single region of various sizes (>0) can be created", [&]() {
			const auto region_size = *rc::gen::inRange<std::size_t>(1UL, TEST_DEFAULT_REGION_SIZE);
			std::vector<std::string> data;

			if (region_size > REGION_METADATA_SIZE) {
				/* if possible, append a single value of size = almost whole region_size */
				data.emplace_back(*rc::gen::container<std::string>(region_size - REGION_METADATA_SIZE,
										   rc::gen::character<char>()));
			}

			auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
			auto region = initialize_stream_single_region(stream.get(), region_size, data);
			verify(stream.get(), region, data, {});

			UT_ASSERTeq(pmemstream_region_free(stream.get(), region), 0);
		});

		ret += rc::check(
			"verify if a region_iterator finds the only region created",
			[&](const std::vector<std::string> &data) {
				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
				auto region =
					initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE, data);
				verify(stream.get(), region, data, {});

				struct pmemstream_region_iterator *riter;
				auto ret = pmemstream_region_iterator_new(&riter, stream.get());
				UT_ASSERTeq(ret, 0);

				struct pmemstream_region r;
				ret = pmemstream_region_iterator_next(riter, &r);
				UT_ASSERTeq(ret, 0);
				UT_ASSERTeq(region.offset, r.offset);
				/* there should be no more regions */
				ret = pmemstream_region_iterator_next(riter, &r);
				UT_ASSERTeq(ret, -1);

				pmemstream_region_iterator_delete(&riter);
				UT_ASSERTeq(pmemstream_region_free(stream.get(), region), 0);
			});

		/* "verify if region of unexpected arbitrary sizes cannot be created" */
		{
			auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
			struct pmemstream_region region;

			std::array sizes{size_t(0), TEST_DEFAULT_STREAM_SIZE + 1UL};
			for (size_t &size : sizes) {
				UT_ASSERT(pmemstream_region_allocate(stream.get(), size, &region) != 0);
			}
		}

		ret += rc::check("verify if a stream of various sizes can be created", [&]() {
			const auto stream_size =
				*rc::gen::inRange<std::size_t>(STREAM_METADATA_SIZE, TEST_DEFAULT_STREAM_SIZE);
			const auto region_size = stream_size - STREAM_METADATA_SIZE;

			RC_PRE(ALIGN_UP(region_size, TEST_DEFAULT_BLOCK_SIZE) <= stream_size);
			RC_PRE(ALIGN_UP(region_size, TEST_DEFAULT_BLOCK_SIZE) > 0);

			auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
			/* and initialize this stream with a single region of */
			auto region = initialize_stream_single_region(stream.get(), region_size, {});
			verify(stream.get(), region, {}, {});

			UT_ASSERTeq(pmemstream_region_free(stream.get(), region), 0);
		});

		ret += rc::check("verify if a stream of various block_sizes can be created", [&]() {
			size_t minimum_block_size = sizeof(span_runtime::entry);
			const auto block_size = *rc::gen::inRange<std::size_t>(
				minimum_block_size, TEST_DEFAULT_STREAM_SIZE / 2UL - STREAM_METADATA_SIZE);

			auto stream = make_pmemstream(path, block_size, TEST_DEFAULT_STREAM_SIZE);
			/* and initialize this stream with a single region of */
			auto region = initialize_stream_single_region(stream.get(), block_size / 10UL, {});
			verify(stream.get(), region, {}, {});

			UT_ASSERTeq(pmemstream_region_free(stream.get(), region), 0);
		});

		/* verify if a stream of block_size = 0 cannot be created */
		{
			try {
				make_pmemstream(path, 0, TEST_DEFAULT_STREAM_SIZE);
				UT_ASSERT_UNREACHABLE;
			} catch (std::runtime_error &e) {
				/* noop */
			} catch (...) {
				UT_ASSERT_UNREACHABLE;
			}
		}
	});
}
