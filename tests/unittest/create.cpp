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
			const auto region_size = *rc::gen::inRange<std::size_t>(1UL, REGION_SIZE);

			/* append a single value of size = generated region_size */
			std::vector<std::string> data;
			const auto c = *rc::gen::character<char>();
			const auto string_size = std::min(1UL, region_size - REGION_METADATA_SIZE);
			data.emplace_back(string_size, c);

			auto stream = make_pmemstream(path, BLOCK_SIZE, STREAM_SIZE);
			auto region = initialize_stream_single_region(stream.get(), region_size, data);
			verify(stream.get(), region, data, {});

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		});

		ret += rc::check("verify if a single region of size = 0 can be created", [&]() {
			auto stream = make_pmemstream(path, BLOCK_SIZE, STREAM_SIZE);
			auto region = initialize_stream_single_region(stream.get(), 0, {});
			verify(stream.get(), region, {}, {});

			/* try to append non-zero entry and expect fail */
			std::string entry("ASDF");
			auto ret =
				pmemstream_append(stream.get(), region, nullptr, entry.data(), entry.size(), nullptr);
			RC_ASSERT(ret != 0);

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		});

		/* XXX: we don't check neither return any valuable return code for this case */
		// ret += rc::check("verify if a region of size > stream_size cannot be created", [&]() {
		// auto stream = make_pmemstream(path, BLOCK_SIZE, STREAM_SIZE);
		// struct pmemstream_region region;
		// RC_ASSERT(pmemstream_region_allocate(stream.get(), STREAM_SIZE + 1UL, &region) != 0);
		// });

		ret += rc::check("verify if a stream of various sizes can be created", [&]() {
			const auto stream_size = *rc::gen::inRange<std::size_t>(STREAM_METADATA_SIZE, STREAM_SIZE);
			const auto region_size = stream_size - STREAM_METADATA_SIZE;

			auto stream = make_pmemstream(path, BLOCK_SIZE, stream_size);
			/* and initialize this stream with a single region of */
			auto region = initialize_stream_single_region(stream.get(), region_size, {});
			verify(stream.get(), region, {}, {});

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		});

		ret += rc::check("verify if a stream of various block_sizes can be created", [&]() {
			const auto block_size =
				*rc::gen::inRange<std::size_t>(1UL, (STREAM_SIZE - STREAM_METADATA_SIZE) / 2UL);

			auto stream = make_pmemstream(path, block_size, STREAM_SIZE);
			/* and initialize this stream with a single region of */
			auto region = initialize_stream_single_region(stream.get(), block_size, {});
			verify(stream.get(), region, {}, {});

			RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
		});

		ret += rc::check("verify if a stream of block_size = 0 cannot be created",
				 [&]() { RC_ASSERT_THROWS(make_pmemstream(path, 0, STREAM_SIZE)); });
	});
}
