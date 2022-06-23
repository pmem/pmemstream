// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* create.cpp -- tests creation of regions and streams with various parameters */

#include <array>
#include <string>
#include <vector>

#include "common/util.h"
#include "rapidcheck_helpers.hpp"
#include "span.h"
#include "stream_helpers.hpp"
#include "unittest.hpp"

namespace
{
std::pair<size_t, size_t> generate_region_size_and_block_size(size_t stream_size)
{
	const auto minimum_block_size = CACHELINE_SIZE;
	const auto max_block_and_region_size = stream_size;

	const auto block_size_pow =
		*rc::gen::inRange<std::size_t>(log2_uint(minimum_block_size), log2_uint(max_block_and_region_size));
	const auto block_size = (1UL << block_size_pow);
	const auto region_size = *rc::gen::inRange<std::size_t>(1, max_block_and_region_size);

	const auto available_size = ALIGN_DOWN(stream_size - std::max(STREAM_METADATA_SIZE, block_size), block_size);
	RC_PRE(ALIGN_UP(region_size + sizeof(struct span_region), block_size) <= available_size);

	return {region_size, block_size};
}
} // namespace

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

		ret += rc::check("verify if a single region of various sizes (>0) can be created", [&]() {
			const auto region_size = *rc::gen::inRange<std::size_t>(1UL, TEST_DEFAULT_REGION_SIZE);
			std::vector<std::string> data;

			if (region_size > REGION_METADATA_SIZE) {
				/* if possible, append a single value of size = almost whole region_size */
				data.emplace_back(*rc::gen::container<std::string>(region_size - REGION_METADATA_SIZE,
										   rc::gen::character<char>()));
			}

			pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
						    get_test_config().stream_size);
			auto region = stream.helpers.initialize_single_region(region_size, data);
			stream.helpers.verify(region, data, {});
		});

		ret += rc::check(
			"verify if a region_iterator finds the only region created",
			[&](const std::vector<std::string> &data) {
				pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
							    get_test_config().stream_size);
				auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, data);
				stream.helpers.verify(region, data, {});

				auto riter = stream.sut.region_iterator();
				pmemstream_region_iterator_seek_first(riter.get());
				int ret = pmemstream_region_iterator_is_valid(riter.get());
				UT_ASSERTeq(ret, 0);
				struct pmemstream_region r = pmemstream_region_iterator_get(riter.get());
				UT_ASSERTeq(region.offset, r.offset);
				/* there should be no more regions */
				pmemstream_region_iterator_next(riter.get());
				ret = pmemstream_region_iterator_is_valid(riter.get());
				UT_ASSERTeq(ret, -1);
			});

		/* "verify if region of unexpected arbitrary sizes cannot be created" */
		{
			pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
						    get_test_config().stream_size);
			std::array sizes{size_t(0), get_test_config().stream_size + 1UL};
			for (size_t &size : sizes) {
				auto [ret, region] = stream.sut.region_allocate(size);
				UT_ASSERTne(ret, 0);
			}
		}

		ret += rc::check("verify if a stream of various sizes can be created", [&]() {
			const auto stream_size =
				*rc::gen::inRange<std::size_t>(STREAM_METADATA_SIZE, get_test_config().stream_size);
			const auto region_size = stream_size - STREAM_METADATA_SIZE;

			RC_PRE(ALIGN_UP(region_size, get_test_config().block_size) + get_test_config().block_size <=
			       stream_size);
			RC_PRE(ALIGN_UP(region_size, get_test_config().block_size) > 0);

			pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
						    stream_size);
			/* and initialize this stream with a single region of */
			auto region = stream.helpers.initialize_single_region(region_size, {});
			stream.helpers.verify(region, {}, {});
		});

		ret += rc::check("verify if a stream of various block_sizes can be created", [&]() {
			auto [region_size, block_size] =
				generate_region_size_and_block_size(get_test_config().stream_size);
			pmemstream_test_base stream(get_test_config().filename, block_size,
						    get_test_config().stream_size);
			/* and initialize this stream with a single region of */
			auto region = stream.helpers.initialize_single_region(region_size, {});
			stream.helpers.verify(region, {}, {});
		});

		ret += rc::check("verify if a region has expected size", [&]() {
			auto [region_size, block_size] =
				generate_region_size_and_block_size(get_test_config().stream_size);
			pmemstream_test_base stream(get_test_config().filename, block_size,
						    get_test_config().stream_size);
			/* and initialize this stream with a single region of */
			auto region = stream.helpers.initialize_single_region(region_size, {});
			size_t expected_region_size = ALIGN_UP(region_size + sizeof(struct span_region), block_size) -
				sizeof(struct span_region);
			UT_ASSERTeq(stream.sut.region_size(region), expected_region_size);
		});

		ret += rc::check(
			"verify that stream can only be created with block_size which is a multiple of CACHELINE_SIZE and a power of 2",
			[&]() {
				auto block_size = *rc::gen::inRange<std::size_t>(
					1ULL, get_test_config().stream_size / 2UL - STREAM_METADATA_SIZE);
				RC_PRE(block_size % CACHELINE_SIZE != 0 || !IS_POW2(block_size));

				try {
					make_pmemstream(get_test_config().filename, block_size,
							get_test_config().stream_size);
					UT_ASSERT_UNREACHABLE;
				} catch (std::runtime_error &e) {
					UT_ASSERT(std::string(e.what()) == "pmemstream_from_map failed");
				} catch (...) {
					UT_ASSERT_UNREACHABLE;
				}
			});

		/* verify if a stream of block_size = 0 cannot be created */
		{
			try {
				make_pmemstream(get_test_config().filename, 0, get_test_config().stream_size);
				UT_ASSERT_UNREACHABLE;
			} catch (std::runtime_error &e) {
				UT_ASSERT(std::string(e.what()) == "pmemstream_from_map failed");
			} catch (...) {
				UT_ASSERT_UNREACHABLE;
			}
		}

		/* XXX: extend map_open helper to properly use stream size and add test similar to this one */
		/* verify if a stream cannot be reopened with different block_size */
		{
			try {
				make_pmemstream(get_test_config().filename, get_test_config().block_size,
						get_test_config().stream_size);
				make_pmemstream(get_test_config().filename, get_test_config().block_size + 1,
						get_test_config().stream_size);
				UT_ASSERT_UNREACHABLE;
			} catch (std::runtime_error &e) {
				UT_ASSERT(std::string(e.what()) == "pmemstream_from_map failed");
			} catch (...) {
				UT_ASSERT_UNREACHABLE;
			}
		}

		ret += rc::check("verify if region deallocated using region_free is not returned by region iterator",
				 [&](const std::string &entry) {
					 auto [region_size, block_size] =
						 generate_region_size_and_block_size(get_test_config().stream_size);

					 {
						 pmemstream_test_base stream(get_test_config().filename, block_size,
									     get_test_config().stream_size);

						 auto region =
							 stream.helpers.initialize_single_region(region_size, {entry});
						 RC_PRE(entry.size() + sizeof(span_entry) <
							stream.sut.region_usable_size(region));

						 stream.helpers.verify(region, {entry}, {});

						 UT_ASSERTeq(stream.helpers.count_regions(), 1);
						 stream.sut.region_free(region);
						 UT_ASSERTeq(stream.helpers.count_regions(), 0);
					 }

					 {
						 pmemstream_test_base stream(get_test_config().filename, block_size,
									     get_test_config().stream_size);
						 UT_ASSERTeq(stream.helpers.count_regions(), 0);
					 }
				 });

		ret += rc::check("verify if we can repopulate stream with the same data after region reallocate",
				 [&](pmemstream_empty &&stream, const std::vector<std::string> &data) {
					 {
						 auto region = stream.helpers.initialize_single_region(
							 get_test_config().region_size, data);

						 UT_ASSERTeq(stream.helpers.count_regions(), 1);
						 stream.sut.region_free(region);
						 UT_ASSERTeq(stream.helpers.count_regions(), 0);

						 auto new_region = stream.helpers.initialize_single_region(
							 get_test_config().region_size, data);
						 UT_ASSERTeq(stream.helpers.count_regions(), 1);
						 UT_ASSERTeq(region.offset, new_region.offset);
						 stream.helpers.verify(new_region, data, {});
					 }
				 });
	});
}
