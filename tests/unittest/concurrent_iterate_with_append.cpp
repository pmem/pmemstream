// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t concurrency = 4;
static constexpr size_t max_size = 1024; /* Max number of elements in stream and max size of single entry. */
static constexpr size_t stream_size = max_size * max_size * concurrency * 10 /* 10x-margin */;
static constexpr size_t block_size = stream_size - STREAM_METADATA_SIZE;

namespace
{
void concurrent_iterate_verify(pmemstream *stream, pmemstream_region region, const std::vector<std::string> &data,
			       const std::vector<std::string> &extra_data)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	RC_ASSERT(pmemstream_entry_iterator_new(&eiter, stream, region) == 0);

	struct pmemstream_entry entry;

	/* Loop until all entires are found. */
	while (result.size() < data.size() + extra_data.size()) {
		int next = pmemstream_entry_iterator_next(eiter, NULL, &entry);
		if (next == 0) {
			auto data_ptr = reinterpret_cast<const char *>(pmemstream_entry_data(stream, entry));
			result.emplace_back(data_ptr, pmemstream_entry_length(stream, entry));
		}
	}

	RC_ASSERT(std::equal(data.begin(), data.end(), result.begin()));
	RC_ASSERT(
		std::equal(extra_data.begin(), extra_data.end(), result.begin() + static_cast<long long>(data.size())));

	pmemstream_entry_iterator_delete(&eiter);
}
} // namespace

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " path" << std::endl;
		return -1;
	}

	auto path = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		/* Disable shrinking and set max_size of entries. */
		/* XXX: can we do though rapidcheck API? */
		std::string rapidcheck_config = "noshrink=1\" max_size=\"" + std::to_string(max_size) + "\"";
		int r = setenv("RC_PARAMS", rapidcheck_config.data(), 0);
		UT_ASSERT(r == 0);

		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage (no preinitialization)",
			[&](std::vector<std::string> &&extra_data, bool use_region_runtime) {
				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
				auto region = initialize_stream_single_region(stream.get(), block_size, {});

				parallel_exec(concurrency, [&](size_t tid) {
					if (tid == 0) {
						/* appender */
						pmemstream_region_runtime *region_runtime = nullptr;
						if (use_region_runtime) {
							pmemstream_get_region_runtime(stream.get(), region,
										      &region_runtime);
						}
						append(stream.get(), region, region_runtime, extra_data);
						verify(stream.get(), region, extra_data, {});
					} else {
						/* iterators */
						concurrent_iterate_verify(stream.get(), region, extra_data, {});
					}
				});
			});

		ret += rc::check("verify if iterators concurrent to append work do not return garbage",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
				     bool use_region_runtime) {
					 auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
					 auto region = initialize_stream_single_region(stream.get(), block_size, data);

					 parallel_exec(concurrency, [&](size_t tid) {
						 if (tid == 0) {
							 /* appender */
							 pmemstream_region_runtime *region_runtime = nullptr;
							 if (use_region_runtime) {
								 pmemstream_get_region_runtime(stream.get(), region,
											       &region_runtime);
							 }
							 append(stream.get(), region, region_runtime, extra_data);
							 verify(stream.get(), region, data, extra_data);
						 } else {
							 /* iterators */
							 concurrent_iterate_verify(stream.get(), region, data,
										   extra_data);
						 }
					 });
				 });

		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage after reopen",
			[&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
			    bool use_region_runtime) {
				pmemstream_region region;
				{
					auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
					region = initialize_stream_single_region(stream.get(), block_size, data);
				}

				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size, false);
				std::vector parallel_exec(concurrency, [&](size_t tid) {
					if (tid == 0) {
						/* appender */
						pmemstream_region_runtime *region_runtime = nullptr;
						if (use_region_runtime) {
							pmemstream_get_region_runtime(stream.get(), region,
										      &region_runtime);
						}
						append(stream.get(), region, region_runtime, extra_data);
						verify(stream.get(), region, data, extra_data);
					} else {
						/* iterators */
						concurrent_iterate_verify(stream.get(), region, data, extra_data);
					}
				});
			});
	});
}
