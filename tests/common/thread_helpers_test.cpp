// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* verifies correctness of thread_helpers */

#include <atomic>

#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
static constexpr size_t concurrency = 128;
} // namespace

int main()
{
	struct test_config_type test_config;
	return run_test(test_config, [] {
		std::atomic<size_t> counter;
		counter = 0;

		syncthreads_barrier syncthreads(concurrency);
		parallel_exec(concurrency, [&](size_t id) {
			counter++;

			syncthreads();
			UT_ASSERTeq(counter.load(), concurrency);
			syncthreads();

			counter++;

			syncthreads();
			UT_ASSERTeq(counter.load(), concurrency * 2);
			syncthreads();
			UT_ASSERTeq(counter.load(), concurrency * 2);
		});

		setenv("PMEMSTREAM_HANDLE_SIGNAL_FOR_DEBUG", "1", 1);
		try {
			parallel_exec(2, [&](size_t id) {
				auto ptr = (char *)nullptr;
				std::cout << *ptr;
				UT_ASSERT_UNREACHABLE;
			});
		} catch (std::runtime_error &e) {
		} catch (...) {
			UT_ASSERT_UNREACHABLE;
		}
	});
}
