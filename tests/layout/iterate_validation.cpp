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

#include "common/util.h"
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

	auto path = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		ret += rc::check(
			"verify if stream does not treat inconsistent spans as valid entries",
			[&](const std::vector<std::string> &data, bool entry_span) {
				RC_PRE(data.size() > 0);

				pmemstream_region region;
				{
					auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE,
								      TEST_DEFAULT_STREAM_SIZE);
					region = initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE,
										 data);

					std::vector<std::string> result;

					struct pmemstream_entry_iterator *eiter;
					UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream.get(), region), 0);

					struct pmemstream_entry entry = {UINT64_MAX};
					while (pmemstream_entry_iterator_next(eiter, nullptr, &entry) == 0) {
						/* NOP */
					}
					UT_ASSERTne(entry.offset, UINT64_MAX);

					pmemstream_entry_iterator_delete(&eiter);

					auto next_entry_offset = ALIGN_UP(
						entry.offset + data.back().size() + sizeof(struct span_entry), 8ULL);
					/* This pointer is not safe to read - it points to uninitialized data */
					auto data_ptr =
						reinterpret_cast<char *>(stream->data.spans) + next_entry_offset;

					auto partial_span =
						generate_inconsistent_span(entry_span ? SPAN_ENTRY : SPAN_EMPTY);
					auto partial_span_ptr = reinterpret_cast<char *>(partial_span.data());
					std::memcpy(static_cast<char *>(data_ptr), partial_span_ptr,
						    partial_span.size() * sizeof(partial_span[0]));
				}
				{
					auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE,
								      TEST_DEFAULT_STREAM_SIZE, false);
					auto stream_data = get_elements_in_region(stream.get(), region);
					UT_ASSERT(std::equal(data.begin(), data.end(), stream_data.begin()));
					UT_ASSERTeq(pmemstream_region_free(stream.get(), region), 0);
				}
			});
	});
}
