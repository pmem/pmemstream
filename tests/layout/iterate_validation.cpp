// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * iterate_validation.cpp -- verifies if randomly generated (inconsistent) spans are not
 *                           treated by pmemstream iterators as valid entries.
 */

#include <cstring>
#include <string>
#include <vector>

#include <rapidcheck.h>

#include "libpmemstream_internal.h"
#include "span.h"
#include "stream_helpers.hpp"
#include "unittest.hpp"

namespace
{
std::vector<uint64_t> generate_inconsistent_span(span_type stype)
{
	static constexpr size_t max_entry_size = 1024;

	size_t metadata_size = 0;
	if (stype == SPAN_EMPTY) {
		metadata_size = sizeof(struct span_empty);
	} else if (stype == SPAN_ENTRY) {
		metadata_size = sizeof(struct span_entry);
	}

	/* Use uint64_t to ensure proper alignment. */
	std::vector<uint64_t> garbage = *rc::gen::container<std::vector<uint64_t>>(
		(max_entry_size + metadata_size) / sizeof(uint64_t), rc::gen::arbitrary<uint64_t>());
	auto size = *rc::gen::inRange<size_t>(0, max_entry_size);

	garbage[0] = size | static_cast<uint64_t>(stype);
	if (stype == SPAN_ENTRY) {
		/* Always wrong popcount. */
		auto popcount = *rc::gen::inRange<size_t>(max_entry_size + 1, std::numeric_limits<size_t>::max());
		garbage[1] = popcount;
	}

	return garbage;
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

		ret += rc::check(
			"verify if stream does not treat inconsistent spans as valid entries",
			[&](const std::vector<std::string> &data, bool entry_span) {
				{
					RC_PRE(data.size() > 0);

					pmemstream_test_base stream(get_test_config().filename,
								    get_test_config().block_size,
								    get_test_config().stream_size);
					auto region =
						stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, data);

					std::vector<std::string> result;

					auto eiter = stream.sut.entry_iterator(region);
					struct pmemstream_entry entry;
					char *base_ptr = nullptr;
					while (pmemstream_entry_iterator_next(eiter.get(), nullptr, &entry) == 0) {
						if (!base_ptr) {
							auto ptr = stream.sut.get_entry(entry).data() - entry.offset;
							base_ptr = const_cast<char *>(ptr);
						}
					}

					/* This pointer is not safe to read - it points to uninitialized data */
					auto data_ptr = base_ptr + entry.offset;
					auto partial_span =
						generate_inconsistent_span(entry_span ? SPAN_ENTRY : SPAN_EMPTY);
					auto partial_span_ptr = reinterpret_cast<char *>(partial_span.data());
					std::memcpy(static_cast<char *>(data_ptr), partial_span_ptr,
						    partial_span.size() * sizeof(partial_span[0]));

					stream.reopen();

					auto stream_data = stream.helpers.get_elements_in_region(region);
					UT_ASSERT(std::equal(data.begin(), data.end(), stream_data.begin()));
				}
			});
	});
}
