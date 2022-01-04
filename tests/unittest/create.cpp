// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* create.cpp -- tests creation of regions and streams with various parameters */

#include <string>
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

		ret += rc::check("verify if a single region of various sizes (>0) can be created", [&]() {
			const auto region_size = *rc::gen::inRange<std::size_t>(1UL, TEST_DEFAULT_REGION_SIZE);
			std::vector<std::string> data;

			if (region_size > REGION_METADATA_SIZE) {
				/* if possible, append a single value of size = almost whole region_size */
				const auto c = *rc::gen::character<char>();
				data.emplace_back(region_size - REGION_METADATA_SIZE, c);
			}

			auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
			auto region = initialize_stream_single_region(stream.get(), region_size, data);
			verify(stream.get(), region, data, {});

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		});

		/* verify if a single region of size = 0 can be created */
		{
			auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
			auto region = initialize_stream_single_region(stream.get(), 0, {});
			verify(stream.get(), region, {}, {});

			/* try to append non-zero entry and expect fail */
			std::string entry("ASDF");
			auto ret =
				pmemstream_append(stream.get(), region, nullptr, entry.data(), entry.size(), nullptr);
			RC_ASSERT(ret != 0);

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		}

		/* XXX: we don't check neither return any valuable return code for this case */
		// ret += rc::check("verify if a region of size > stream_size cannot be created", [&]() {
		// auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
		// struct pmemstream_region region;
		// RC_ASSERT(pmemstream_region_allocate(stream.get(), TEST_DEFAULT_STREAM_SIZE + 1UL, &region) != 0);
		// });

		ret += rc::check("verify if a stream of various sizes can be created", [&]() {
			const auto stream_size =
				*rc::gen::inRange<std::size_t>(STREAM_METADATA_SIZE, TEST_DEFAULT_STREAM_SIZE);
			const auto region_size = stream_size - STREAM_METADATA_SIZE;

			auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
			/* and initialize this stream with a single region of */
			auto region = initialize_stream_single_region(stream.get(), region_size, {});
			verify(stream.get(), region, {}, {});

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		});

		ret += rc::check("verify if a stream of various block_sizes can be created", [&]() {
			const auto block_size = *rc::gen::inRange<std::size_t>(
				1UL, (TEST_DEFAULT_STREAM_SIZE - STREAM_METADATA_SIZE) / 2UL);

			auto stream = make_pmemstream(path, block_size, TEST_DEFAULT_STREAM_SIZE);
			/* and initialize this stream with a single region of */
			auto region = initialize_stream_single_region(stream.get(), block_size, {});
			verify(stream.get(), region, {}, {});

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		});

		/* verify if a stream of block_size = 0 cannot be created */
		// {
		// try {
		// make_pmemstream(path, 0, TEST_DEFAULT_STREAM_SIZE);
		// UT_ASSERT_UNREACHABLE;
		// } catch (std::runtime_error &e) {
		// /* noop */
		// } catch (...) {
		// UT_ASSERT_UNREACHABLE;
		// }
		// }
	});
}
