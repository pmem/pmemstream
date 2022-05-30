// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "env_setter.hpp"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t max_concurrency = 4;
static constexpr size_t max_size = 1024; /* Max number of elements in stream and max size of single entry. */
static constexpr size_t stream_size = max_size * max_size * max_concurrency * 10 /* 10x-margin */;
static constexpr size_t region_size = stream_size - STREAM_METADATA_SIZE;

namespace
{
void concurrent_iterate_verify(pmemstream_test_base &stream, pmemstream_region region,
			       const std::vector<std::string> &data, const std::vector<std::string> &extra_data)
{
	std::vector<std::string> result;

	auto eiter = stream.sut.entry_iterator(region);

	while (pmemstream_entry_iterator_is_valid(eiter.get()) != 0)
		pmemstream_entry_iterator_seek_first(eiter.get());

	uint64_t expected_timestamp = 1;

	/* Loop until all entries are found. */
	while (result.size() < data.size() + extra_data.size()) {
		if (pmemstream_entry_iterator_is_valid(eiter.get()) == 0) {
			struct pmemstream_entry entry = pmemstream_entry_iterator_get(eiter.get());

			uint64_t timestamp = pmemstream_entry_timestamp(stream.sut.c_ptr(), entry);
			UT_ASSERTeq(timestamp, expected_timestamp);
			expected_timestamp++;

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
		       const std::vector<std::string> &extra_data, bool reopen, size_t concurrency, bool async)
{
	auto region = stream.helpers.get_first_region();

	if (reopen)
		stream.reopen();

	parallel_exec(concurrency, [&](size_t tid) {
		if (tid == 0) {
			/* appender */
			if (async)
				stream.helpers.async_append(region, extra_data);
			else
				stream.helpers.append(region, extra_data);

			stream.helpers.verify(region, data, extra_data);
		} else {
			/* iterators */
			concurrent_iterate_verify(stream, region, data, extra_data);
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

	return run_test(test_config, [&] {
		return_check ret;

		/* Set max_size of entries (if this test is failing, consider setting also 'noshrink=1';
		 * shrinking may not work due to non-deterministic nature of concurrent tests). */
		/* XXX: can we do this via rapidcheck API? */
		std::string rapidcheck_config = "max_size=" + std::to_string(max_size);
		env_setter setter("RC_PARAMS", rapidcheck_config, false);

		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage (no preinitialization)",
			[&](pmemstream_empty &&stream, const std::vector<std::string> &extra_data, bool reopen,
			    ranged<size_t, 1, max_concurrency> concurrency, bool async) {
				RC_PRE(extra_data.size() > 0);
				stream.helpers.initialize_single_region(region_size, {});
				verify_no_garbage(std::move(stream), {}, extra_data, reopen, concurrency, async);
			});

		ret += rc::check("verify if iterators concurrent to append work do not return garbage ",
				 [&](pmemstream_empty &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, bool reopen,
				     ranged<size_t, 1, max_concurrency> concurrency, bool async) {
					 RC_PRE(data.size() + extra_data.size() > 0);
					 stream.helpers.initialize_single_region(region_size, data);
					 verify_no_garbage(std::move(stream), data, extra_data, reopen, concurrency,
							   async);
				 });
	});
}
