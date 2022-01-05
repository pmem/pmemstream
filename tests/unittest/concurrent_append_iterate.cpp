// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t stream_size = 1024 * 1024;
static constexpr size_t concurrency = 4;

namespace
{
void concurrent_iterate_verify(pmemstream *stream, pmemstream_region region, const std::vector<std::string> &data,
			       const std::vector<std::string> &extra_data)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	RC_ASSERT(pmemstream_entry_iterator_new(&eiter, stream, region) == 0);

	struct pmemstream_entry entry;
	while (result.size() < data.size() + extra_data.size()) {
		int next = pmemstream_entry_iterator_next(eiter, NULL, &entry);
		if (next == 0) {
			auto data_ptr = reinterpret_cast<char *>(pmemstream_entry_data(stream, entry));
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
		std::cout << "Usage: " << argv[0] << " file" << std::endl;
		return -1;
	}

	auto file = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		// XXX: add test with larger data sizes
		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage (no preinitialization)",
			[&](std::vector<std::string> &&extra_data) {
				static constexpr size_t region_size = stream_size - 16 * 1024;
				static constexpr size_t block_size = 4096;

				auto stream = make_pmemstream(file, block_size, stream_size);
				auto region = initialize_stream_single_region(stream.get(), region_size, {});

				parallel_exec(concurrency, [&](size_t tid) {
					if (tid == 0) {
						/* appender */
						append(stream.get(), region, NULL, extra_data);
						verify(stream.get(), region, extra_data, {});
					} else {
						/* iterators */
						concurrent_iterate_verify(stream.get(), region, extra_data, {});
					}
				});
				RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
			});

		ret += rc::check("verify if iterators concurrent to append work do not return garbage",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data) {
					 static constexpr size_t region_size = stream_size - 16 * 1024;
					 static constexpr size_t block_size = 4096;

					 auto stream = make_pmemstream(file, block_size, stream_size);
					 auto region = initialize_stream_single_region(stream.get(), region_size, data);

					 parallel_exec(concurrency, [&](size_t tid) {
						 if (tid == 0) {
							 /* appender */
							 append(stream.get(), region, NULL, extra_data);
							 verify(stream.get(), region, data, extra_data);
						 } else {
							 /* iterators */
							 concurrent_iterate_verify(stream.get(), region, data,
										   extra_data);
						 }
					 });
					 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
				 });

		ret += rc::check(
			"verify if iterators concurrent to append work do not return garbage after reopen",
			[&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data) {
				static constexpr size_t region_size = stream_size - 16 * 1024;
				static constexpr size_t block_size = 4096;

				pmemstream_region region;
				{
					auto stream = make_pmemstream(file, block_size, stream_size);
					region = initialize_stream_single_region(stream.get(), region_size, data);
				}

				auto stream = make_pmemstream(file, block_size, stream_size, false);
				std::vector parallel_exec(concurrency, [&](size_t tid) {
					if (tid == 0) {
						/* appender */
						append(stream.get(), region, NULL, extra_data);
						verify(stream.get(), region, data, extra_data);
					} else {
						/* iterators */
						concurrent_iterate_verify(stream.get(), region, data, extra_data);
					}
				});
				RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
			});
	});
}
