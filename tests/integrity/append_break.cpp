// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * append_break.cpp -- pmemstream_append break - data integrity test
 *	XXX: this test could be extended by using random data appended in "iterate_*" phase
 */

#include "libpmemstream_internal.h"
#include "span.h"
#include "stream_helpers.hpp"
#include "stream_span_helpers.hpp"
#include "unittest.hpp"

#include <cstring>
#include <vector>

static constexpr size_t regions_count = 2;

static void test(std::string mode)
{
	std::vector<std::string> init_data;
	init_data.emplace_back("DEADBEEF");
	init_data.emplace_back("NONEMPTYDATA");
	init_data.emplace_back("mydata");

	if (mode == "init") {
		/* append initial data to a new stream */

		pmemstream_test_base s(get_test_config().filename, get_test_config().block_size,
				       get_test_config().stream_size);

		/* initialized multiple regions and append only to the first one */
		s.helpers.initialize_multi_regions(regions_count, get_test_config().region_size, {});
		auto r1 = s.helpers.get_first_region();
		s.helpers.append(r1, init_data);

	} else if (mode == "break") {
		/* break in the middle of an append */

		pmemstream_test_base s(get_test_config().filename, get_test_config().block_size, 0, false);
		auto r1 = s.helpers.get_first_region();

		/* append (gdb script should tear the memcpy) */
		/* add entry longer than 512 */
		std::string buf(1500, '~');
		s.sut.append(r1, buf);
		UT_ASSERT_UNREACHABLE;

	} else if (mode == "iterate_before") {
		/* iterate all entries BEFORE an append */

		pmemstream_test_base s(get_test_config().filename, get_test_config().block_size, 0, false);
		auto r1 = s.helpers.get_first_region();

		/* read back data and count for the same output */
		auto read_elements = s.helpers.get_elements_in_region(r1);
		/* While iterating over all entries, entry torn
		 * in the previous append should be cleared now. */
		auto cnt = read_elements.size();
		UT_ASSERTeq(cnt, init_data.size());
		for (size_t i = 0; i < cnt; ++i) {
			UT_ASSERT(init_data[i] == read_elements[i]);
		}

		auto r2 = s.helpers.get_region(1);
		read_elements = s.helpers.get_elements_in_region(r2);
		cnt = read_elements.size();
		UT_ASSERTeq(cnt, 0);

		/* timestamp should be equal to the number of all elements in the stream */
		auto committed_timestamp = pmemstream_committed_timestamp(s.helpers.stream.c_ptr());
		auto persisted_timestamp = pmemstream_persisted_timestamp(s.helpers.stream.c_ptr());
		UT_ASSERTeq(init_data.size(), committed_timestamp);
		UT_ASSERTeq(init_data.size(), persisted_timestamp);

		/* append new entry to the second (empty) region */
		std::string buf(128, 'A');
		s.sut.append(r2, buf);

		/* we're using regular append here, so both timestamps should be immediately updated */
		committed_timestamp = pmemstream_committed_timestamp(s.helpers.stream.c_ptr());
		persisted_timestamp = pmemstream_persisted_timestamp(s.helpers.stream.c_ptr());
		UT_ASSERTeq(init_data.size() + 1, committed_timestamp);
		UT_ASSERTeq(init_data.size() + 1, persisted_timestamp);

		/* iterate, recover (underneath) and make sure the entries count is as expected */
		read_elements = s.helpers.get_elements_in_region(r1);
		cnt = read_elements.size();
		UT_ASSERTeq(cnt, init_data.size());

		read_elements = s.helpers.get_elements_in_region(r2);
		cnt = read_elements.size();
		UT_ASSERTeq(cnt, 1);
		UT_ASSERT(read_elements.back() == buf);

	} else if (mode == "iterate_after") {
		/* iterate all entries AFTER an append */

		pmemstream_test_base s(get_test_config().filename, get_test_config().block_size, 0, false);

		auto committed_timestamp = pmemstream_committed_timestamp(s.helpers.stream.c_ptr());
		auto persisted_timestamp = pmemstream_persisted_timestamp(s.helpers.stream.c_ptr());
		UT_ASSERTeq(init_data.size(), committed_timestamp);
		UT_ASSERTeq(init_data.size(), persisted_timestamp);

		/* append new entry to the second (empty) region */
		auto r2 = s.helpers.get_region(1);
		std::string buf(100, 'A');
		s.sut.append(r2, buf);

		/* we're using regular append here, so both timestamps should be immediately updated */
		committed_timestamp = pmemstream_committed_timestamp(s.helpers.stream.c_ptr());
		persisted_timestamp = pmemstream_persisted_timestamp(s.helpers.stream.c_ptr());
		UT_ASSERTeq(init_data.size() + 1, committed_timestamp);
		UT_ASSERTeq(init_data.size() + 1, persisted_timestamp);

		/* check if we have for sure a duplicated timestamp */
		auto regions = span_runtimes_from_stream(s.sut, 0, UINT64_MAX);

		/* in the 1. region there should be 4 entry spans (incl. broken one) + potentially 1 empty span */
		UT_ASSERT(regions[0].sub_spans.size() >= 4);
		auto broken_entry_in_r1 = (struct span_entry *)span_offset_to_span_ptr(&s.sut.c_ptr()->data,
										       regions[0].sub_spans[3].offset);
		UT_ASSERT(span_get_type(&broken_entry_in_r1->span_base) == SPAN_ENTRY);

		/* in the 2. region there have to be 1 entry + 1 empty span (we clear out 1 span after an append) */
		UT_ASSERTeq(regions[1].sub_spans.size(), 2);
		auto new_entry_in_r2 = (struct span_entry *)span_offset_to_span_ptr(&s.sut.c_ptr()->data,
										    regions[1].sub_spans[0].offset);
		UT_ASSERT(span_get_type(&new_entry_in_r2->span_base) == SPAN_ENTRY);
		UT_ASSERTeq(broken_entry_in_r1->timestamp, new_entry_in_r2->timestamp);

		/* iterate, recover (underneath) and make sure the entries' count is as expected */
		auto read_elements = s.helpers.get_elements_in_region(r2);
		auto cnt = read_elements.size();
		UT_ASSERTeq(cnt, 1);
		UT_ASSERT(read_elements.back() == buf);

		auto r1 = s.helpers.get_first_region();
		read_elements = s.helpers.get_elements_in_region(r1);
		cnt = read_elements.size();
		UT_ASSERTeq(cnt, init_data.size());

		/* recovery done, so another new entry in the 1. region should be right next to
		 * the initial 3 entries and should have the next proper timestamp */
		s.sut.append(r1, buf);

		committed_timestamp = pmemstream_committed_timestamp(s.helpers.stream.c_ptr());
		persisted_timestamp = pmemstream_persisted_timestamp(s.helpers.stream.c_ptr());
		UT_ASSERTeq(init_data.size() + 2, committed_timestamp);
		UT_ASSERTeq(init_data.size() + 2, persisted_timestamp);

		/* check if new entries (in both regions) have proper timestamps and each of these
		 * entries are now followed by an empty span */
		regions = span_runtimes_from_stream(s.sut, 0, UINT64_MAX);

		/* 1. region: 4 entry spans + 1 empty span (+ possible trash) */
		UT_ASSERT(regions[0].sub_spans.size() >= 5);
		auto new_entry_in_r1 = (struct span_entry *)span_offset_to_span_ptr(&s.sut.c_ptr()->data,
										    regions[0].sub_spans[3].offset);
		UT_ASSERT(span_get_type(&new_entry_in_r1->span_base) == SPAN_ENTRY);
		auto next_empty_in_r1 = (struct span_base *)span_offset_to_span_ptr(&s.sut.c_ptr()->data,
										    regions[0].sub_spans[4].offset);
		UT_ASSERT(span_get_type(next_empty_in_r1) == SPAN_EMPTY);

		/* 2. region: 1 entry span + 1 empty span */
		UT_ASSERTeq(regions[1].sub_spans.size(), 2);
		new_entry_in_r2 = (struct span_entry *)span_offset_to_span_ptr(&s.sut.c_ptr()->data,
									       regions[1].sub_spans[0].offset);
		UT_ASSERT(span_get_type(&new_entry_in_r2->span_base) == SPAN_ENTRY);
		auto next_empty_in_r2 = (struct span_base *)span_offset_to_span_ptr(&s.sut.c_ptr()->data,
										    regions[1].sub_spans[1].offset);
		UT_ASSERT(span_get_type(next_empty_in_r2) == SPAN_EMPTY);

		UT_ASSERTeq(new_entry_in_r1->timestamp, persisted_timestamp);
		UT_ASSERTeq(new_entry_in_r1->timestamp, new_entry_in_r2->timestamp + 1);

		read_elements = s.helpers.get_elements_in_region(r1);
		cnt = read_elements.size();
		UT_ASSERTeq(cnt, init_data.size() + 1);
		UT_ASSERT(read_elements.back() == buf);

	} else {
		std::cout << "Wrong mode given!" << std::endl;
		UT_ASSERT_UNREACHABLE;
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << " <init|break|iterate_before|iterate_after> file-path"
			  << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[2]);

	std::string mode = argv[1];
	return run_test(test_config, [&] { test(mode); });
}
