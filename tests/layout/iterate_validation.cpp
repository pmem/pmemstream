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
#include "rapidcheck_helpers.hpp"
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
		/* Always wrong timestamp. */
		auto popcount = std::numeric_limits<uint64_t>::max();
		garbage[1] = popcount;
	}

	return garbage;
}

std::vector<uint64_t> generate_consistent_entry_span(uint64_t timestamp)
{
	static constexpr size_t max_entry_size = 1024;

	size_t metadata_size = sizeof(struct span_entry);
	auto size = *rc::gen::inRange<size_t>(0, max_entry_size);

	std::vector<uint64_t> data = *rc::gen::container<std::vector<uint64_t>>(
		(metadata_size + size + sizeof(uint64_t)) / sizeof(uint64_t), rc::gen::arbitrary<uint64_t>());

	data[0] = size | static_cast<uint64_t>(SPAN_ENTRY);
	data[1] = timestamp;

	return data;
}

void write_custom_span_at_tail(pmemstream_test_base &stream, pmemstream_region region,
			       const std::vector<uint64_t> &span)
{
	auto span_data_size = span[0] & SPAN_EXTRA_MASK;

	/* Valid append */
	auto [ret, entry] = stream.sut.append(region, std::string(span_data_size, 'x'));
	UT_ASSERTeq(ret, 0);

	auto entry_view = stream.sut.get_entry(entry);
	auto span_ptr = reinterpret_cast<const char *>(entry_view.data()) - sizeof(span_entry);

	/* Now, overwrite the valid entry in stream with custom one. */
	std::memcpy(const_cast<char *>(span_ptr), span.data(), span_data_size + sizeof(span_entry));
	stream.sut.c_ptr()->data.persist(span_ptr, span_data_size + sizeof(span_entry));
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

		ret += rc::check("verify if stream does not treat inconsistent spans as valid entries",
				 [&](pmemstream_empty &&stream, const std::vector<std::string> &data, bool entry_span) {
					 auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE,
											       data);

					 auto span = generate_inconsistent_span(entry_span ? SPAN_ENTRY : SPAN_EMPTY);
					 write_custom_span_at_tail(stream, region, span);

					 stream.reopen();

					 auto stream_data = stream.helpers.get_elements_in_region(region);
					 UT_ASSERTeq(stream_data.size(), data.size());
					 UT_ASSERT(std::equal(data.begin(), data.end(), stream_data.begin()));
				 });

		ret += rc::check("verify if stream does treat consistent entry spans as valid entries",
				 [&](pmemstream_empty &&stream, const std::vector<std::string> &data) {
					 auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE,
											       data);

					 auto span = generate_consistent_entry_span(
						 stream.helpers.get_elements_in_region(region).size());
					 write_custom_span_at_tail(stream, region, span);

					 stream.reopen();

					 auto stream_data = stream.helpers.get_elements_in_region(region);
					 UT_ASSERTeq(stream_data.size(), data.size() + 1);
					 UT_ASSERT(std::equal(data.begin(), data.end(), stream_data.begin()));
				 });
	});
}
