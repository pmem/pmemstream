// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* thread_id.cpp -- verifies correctness of thread_id module */

#include "thread_id.h"

#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
static constexpr size_t concurrency = 128;

auto make_thread_id = make_instance_ctor(thread_id_new, thread_id_destroy);
} // namespace

int main()
{
	return run_test([] {
		/* verify if max thread id is not bigger than number of threads */
		{
			auto thread_id = make_thread_id();

			std::vector<size_t> thread_data(concurrency);
			parallel_exec(concurrency,
				      [&](size_t id) { thread_data[id] = thread_id_get(thread_id.get()); });

			std::sort(thread_data.begin(), thread_data.end());
			UT_ASSERT(thread_data.back() <= concurrency);
		};

		/* verify if thread ids are in 0..concurrency range (when all threads are alive). */
		{
			auto thread_id = make_thread_id();

			std::vector<size_t> threads_data(concurrency);
			parallel_xexec(concurrency, [&](size_t id, std::function<void(void)> syncthreads) {
				threads_data[id] = thread_id_get(thread_id.get());

				syncthreads();

				if (id == 0) {
					std::sort(threads_data.begin(), threads_data.end());
					for (size_t i = 0; i < threads_data.size(); i++) {
						UT_ASSERTeq(threads_data[i], i);
					}
				}
			});
		}

		/* verify that thread_id_get() returns the same value as long as thread lives. */
		{
			auto thread_id = make_thread_id();

			parallel_xexec(concurrency, [&](size_t tid, std::function<void(void)> syncthreads) {
				auto id = thread_id_get(thread_id.get());
				UT_ASSERTeq(id, thread_id_get(thread_id.get()));

				syncthreads();

				UT_ASSERTeq(id, thread_id_get(thread_id.get()));
			});
		}

		/* verify that thread id is released after thread dies and can be reused. */
		{
			auto thread_id = make_thread_id();

			parallel_exec(concurrency, [&](size_t id) { thread_id_get(thread_id.get()); });

			UT_ASSERTeq(thread_id_get(thread_id.get()), 0);
		}
	});
}
