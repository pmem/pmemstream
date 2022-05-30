// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "common/util.h"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t min_write_concurrency = 1;
static constexpr size_t max_write_concurrency = 4;
static constexpr size_t read_concurrency = 8;
static constexpr size_t max_size = 1024; /* Max number of elements in stream and max size of single entry. */
static constexpr size_t region_size = ALIGN_UP(max_size * max_size * 10, 4096ULL); /* 10x-margin */
static constexpr size_t stream_size =
	(region_size + REGION_METADATA_SIZE) * max_write_concurrency + STREAM_METADATA_SIZE;

namespace
{
void concurrent_iterate_verify(pmemstream_test_base &stream, pmemstream_region region,
			       const std::vector<std::string> &data, const std::vector<std::string> &extra_data)
{
	std::vector<std::string> result;

	auto eiter = stream.sut.entry_iterator(region);

	while (pmemstream_entry_iterator_is_valid(eiter.get()) != 0)
		pmemstream_entry_iterator_seek_first(eiter.get());

	/* Loop until all entries are found. */
	while (result.size() < data.size() + extra_data.size()) {
		if (pmemstream_entry_iterator_is_valid(eiter.get()) == 0) {
			struct pmemstream_entry entry = pmemstream_entry_iterator_get(eiter.get());
			UT_ASSERT(stream.sut.entry_timestamp(entry) <= stream.sut.committed_timestamp());

			result.emplace_back(stream.sut.get_entry(entry));
			pmemstream_entry_iterator_next(eiter.get());
		}
	}

	UT_ASSERT(std::equal(data.begin(), data.end(), result.begin()));
	auto is_equal =
		std::equal(extra_data.begin(), extra_data.end(), result.begin() + static_cast<long long>(data.size()));
	if (!is_equal) {
		/* tmp: for easier debug */
		auto mismatch = std::mismatch(extra_data.begin(), extra_data.end(),
					      result.begin() + static_cast<long long>(data.size()));
		(void)mismatch;
		UT_ASSERT_UNREACHABLE;
	}
}

void verify_no_garbage(pmemstream_test_base &&stream, const std::vector<std::string> &data,
		       const std::vector<std::string> &extra_data, bool reopen, size_t async_concurrent_appends,
		       size_t sync_concurrent_appends)
{
	auto total_write_concurrency = async_concurrent_appends + sync_concurrent_appends;
	RC_PRE(total_write_concurrency <= max_write_concurrency);
	RC_PRE(total_write_concurrency >= min_write_concurrency);

	std::vector<pmemstream_region> regions;
	// XXX: always initialize for concurrent appends (region_runtime map in helpers is not thread safe)
	stream.call_initialize_region_runtime = true;
	stream.call_initialize_region_runtime_after_reopen = true;
	stream.helpers.call_region_runtime_initialize = true;
	for (size_t i = 0; i < total_write_concurrency; i++) {
		regions.push_back(stream.helpers.initialize_single_region(region_size, data));
	}

	if (reopen)
		stream.reopen();

	parallel_exec(read_concurrency + total_write_concurrency, [&](size_t tid) {
		if (tid < async_concurrent_appends) {
			/* async appender */
			stream.helpers.async_append(regions[tid], extra_data);
		} else if (tid < total_write_concurrency) {
			/* sync appender */
			stream.helpers.append(regions[tid], extra_data);
		} else {
			/* iterators */
			auto read_id = tid - total_write_concurrency;
			concurrent_iterate_verify(stream, regions[read_id % regions.size()], data, extra_data);
		}
	});
}
} // namespace

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.stream_size = stream_size;
	/* Set max_size of entries (if this test is failing, consider setting also 'noshrink=1';
	 * shrinking may not work due to non-deterministic nature of concurrent tests). */
	test_config.rc_params["max_size"] = std::to_string(max_size);

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage (no preinitialization)",
			[&](pmemstream_empty &&stream, const std::vector<std::string> &extra_data, bool reopen,
			    ranged<size_t, 0, max_write_concurrency> async_concurrent_appends,
			    ranged<size_t, 0, max_write_concurrency> sync_concurrent_appends) {
				RC_PRE(extra_data.size() > 0);
				verify_no_garbage(std::move(stream), {}, extra_data, reopen, async_concurrent_appends,
						  sync_concurrent_appends);
			});

		ret += rc::check("verify if iterators concurrent to append work do not return garbage ",
				 [&](pmemstream_empty &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, bool reopen,
				     ranged<size_t, 0, max_write_concurrency> async_concurrent_appends,
				     ranged<size_t, 0, max_write_concurrency> sync_concurrent_appends) {
					 RC_PRE(data.size() + extra_data.size() > 0);
					 verify_no_garbage(std::move(stream), data, extra_data, reopen,
							   async_concurrent_appends, sync_concurrent_appends);
				 });
	});
}
