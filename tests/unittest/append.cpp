// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <algorithm>
#include <bitset>
#include <cstring>
#include <numeric>
#include <vector>

#include <rapidcheck.h>

#include "unittest.hpp"

static constexpr size_t stream_size = 1024 * 1024;

namespace
{
void append_at_offset(struct pmemstream *stream, struct pmemstream_region *region,
		      struct pmemstream_region_context *region_context, const std::vector<std::string> &data)
{
	for (const auto &e : data) {
		auto ret = pmemstream_append(stream, region, region_context, e.data(), e.size(), nullptr);
		RC_ASSERT(ret == 0);
	}
}

struct pmemstream_region initialize_stream_single_region(struct pmemstream *stream, size_t region_size,
							 const std::vector<std::string> &data)
{
	struct pmemstream_region new_region;
	RC_ASSERT(pmemstream_region_allocate(stream, region_size, &new_region) == 0);

	struct pmemstream_entry_iterator *eiter;
	RC_ASSERT(pmemstream_entry_iterator_new(&eiter, stream, new_region) == 0);

	append_at_offset(stream, &new_region, NULL, data);

	return new_region;
}

std::vector<std::string> get_elements_in_region(struct pmemstream *stream, struct pmemstream_region *region)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	RC_ASSERT(pmemstream_entry_iterator_new(&eiter, stream, *region) == 0);

	struct pmemstream_entry entry;
	while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
		auto data_ptr = reinterpret_cast<char *>(pmemstream_entry_data(stream, entry));
		result.emplace_back(data_ptr, pmemstream_entry_length(stream, entry));
	}

	pmemstream_entry_iterator_delete(&eiter);

	return result;
}
} // namespace

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file" << std::endl;
		return -1;
	}

	auto file = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		ret += rc::check("verify if iteration return proper elements after append",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data) {
					 static constexpr size_t region_size = stream_size - 16 * 1024;
					 static constexpr size_t block_size = 4096;
					 auto stream = make_pmemstream(file, block_size, stream_size);

					 /* Allocate region and init it with data */
					 auto region = initialize_stream_single_region(stream.get(), region_size, data);

					 /* Verify that all data matches */
					 RC_ASSERT(get_elements_in_region(stream.get(), &region) == data);

					 /* Append extra_data to the end */
					 append_at_offset(stream.get(), &region, NULL, extra_data);

					 /* Verify if stream now holds data + extra_data */
					 auto all_elements = get_elements_in_region(stream.get(), &region);
					 auto extra_data_start = all_elements.begin() + static_cast<int>(data.size());

					 RC_ASSERT(std::equal(all_elements.begin(), extra_data_start, data.begin()));
					 RC_ASSERT(
						 std::equal(extra_data_start, all_elements.end(), extra_data.begin()));

					 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
				 });
	});
}
