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
	return run_test([] {
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
	});
}
