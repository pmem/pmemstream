// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "env_setter.hpp"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t concurrency = 6;
static constexpr size_t max_size = 1024; /* Max number of elements in stream and max size of single entry. */
static constexpr size_t stream_size = max_size * max_size * concurrency * 10 /* 10x-margin */;
static constexpr size_t region_size = stream_size - STREAM_METADATA_SIZE;

namespace
{
template <typename VerifyF>
void concurrent_iterate_verify(pmemstream *stream, pmemstream_region region, size_t num_entries, VerifyF &&verify_f)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	UT_ASSERT(pmemstream_entry_iterator_new(&eiter, stream, region) == 0);

	struct pmemstream_entry entry;

	/* Loop until all entries are found. */
	while (result.size() < num_entries) {
		int next = pmemstream_entry_iterator_next(eiter, NULL, &entry);
		if (next == 0) {
			auto data_ptr = reinterpret_cast<const char *>(pmemstream_entry_data(stream, entry));
			result.emplace_back(data_ptr, pmemstream_entry_length(stream, entry));
		}
	}

	verify_f(std::move(result));

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
		/* XXX: can we do this via rapidcheck API? */
		std::string rapidcheck_config = "noshrink=1 max_size=" + std::to_string(max_size);
		env_setter setter("RC_PARAMS", rapidcheck_config, false);

		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage (no preinitialization)",
			[&](std::vector<std::string> &&data, bool use_region_runtime) {
				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
				auto region = initialize_stream_single_region(stream.get(), region_size, {});

				parallel_exec(concurrency, [&](size_t tid) {
					if (tid == 0) {
						/* appender */
						pmemstream_region_runtime *region_runtime = nullptr;
						if (use_region_runtime) {
							pmemstream_region_runtime_initialize(stream.get(), region,
											     &region_runtime);
						}
						append(stream.get(), region, region_runtime, data);
						verify(stream.get(), region, data, {});
					} else {
						/* iterators */
						concurrent_iterate_verify(stream.get(), region, data.size(),
									  [&](const std::vector<std::string> &&result) {
										  UT_ASSERT(std::equal(data.begin(),
												       data.end(),
												       result.begin()));
									  });
					}
				});
			});

		ret += rc::check(
			"verify if iterators run in multiple threads and appends running in multiple threads work correctly",
			[&](std::vector<std::string> &&data, bool use_region_runtime) {
				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
				auto region = initialize_stream_single_region(stream.get(), region_size, {});

				auto num_appenders = *rc::gen::inRange<size_t>(1, concurrency);

				std::vector<std::string> combined_data;
				for (size_t i = 0; i < num_appenders; i++) {
					combined_data.insert(combined_data.end(), data.begin(), data.end());
				}
				std::sort(combined_data.begin(), combined_data.end());

				parallel_exec(concurrency, [&](size_t tid) {
					if (tid < num_appenders) {
						/* appender */
						pmemstream_region_runtime *region_runtime = nullptr;
						if (use_region_runtime) {
							pmemstream_region_runtime_initialize(stream.get(), region,
											     &region_runtime);
						}
						append(stream.get(), region, region_runtime, data);
					} else {
						/* iterators */
						concurrent_iterate_verify(
							stream.get(), region, combined_data.size(),
							[&](std::vector<std::string> &&result) {
								std::sort(result.begin(), result.end());
								UT_ASSERT(std::equal(combined_data.begin(),
										     combined_data.end(),
										     result.begin()));
							});
					}
				});
			});

		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage",
			[&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
			    bool use_region_runtime) {
				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size);
				auto region = initialize_stream_single_region(stream.get(), region_size, data);

				parallel_exec(concurrency, [&](size_t tid) {
					if (tid == 0) {
						/* appender */
						pmemstream_region_runtime *region_runtime = nullptr;
						if (use_region_runtime) {
							pmemstream_region_runtime_initialize(stream.get(), region,
											     &region_runtime);
						}
						append(stream.get(), region, region_runtime, extra_data);
						verify(stream.get(), region, data, extra_data);
					} else {
						/* iterators */
						concurrent_iterate_verify(
							stream.get(), region, data.size() + extra_data.size(),
							[&](const std::vector<std::string> &result) {
								UT_ASSERT(std::equal(data.begin(), data.end(),
										     result.begin()));
								UT_ASSERT(std::equal(
									extra_data.begin(), extra_data.end(),
									result.begin() +
										static_cast<long long>(data.size())));
							});
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
					region = initialize_stream_single_region(stream.get(), region_size, data);
				}

				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, stream_size, false);
				parallel_exec(concurrency, [&](size_t tid) {
					if (tid == 0) {
						/* appender */
						pmemstream_region_runtime *region_runtime = nullptr;
						if (use_region_runtime) {
							pmemstream_region_runtime_initialize(stream.get(), region,
											     &region_runtime);
						}
						append(stream.get(), region, region_runtime, extra_data);
						verify(stream.get(), region, data, extra_data);
					} else {
						/* iterators */
						concurrent_iterate_verify(
							stream.get(), region, data.size() + extra_data.size(),
							[&](const std::vector<std::string> &result) {
								UT_ASSERT(std::equal(data.begin(), data.end(),
										     result.begin()));
								UT_ASSERT(std::equal(
									extra_data.begin(), extra_data.end(),
									result.begin() +
										static_cast<long long>(data.size())));
							});
					}
				});
			});
	});
}
