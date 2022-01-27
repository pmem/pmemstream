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

		/* verify that thread ids are released after all threads die and can be reused. */
		{
			auto thread_id = make_thread_id();

			parallel_exec(concurrency, [&](size_t id) { thread_id_get(thread_id.get()); });

			UT_ASSERTeq(thread_id_get(thread_id.get()), 0);
		}

		/* verify that thread id is released after arbitrary thread dies and can be reused. */
		{
			/* Description:
			 * 1. Create 'concurrency' threads and call thread_id_get in each of them.
			 * 2. After all threads have called thread_id_get, save reference to one of them
			 *    (arbitrary choosen one) and let this one thread finish, keep remaining threads
			 *    alive.
			 * 3. Wait for the arbitrary choosen thread to finish and in different thread, call
			 *    thread_id_get. This should return the exact same id as was returned inside the
			 *    thread which just finished.
			 * 4. Stop all other threads.
			 */
			auto thread_id = make_thread_id();

			static constexpr size_t thread_to_destroy_id = 45;
			static_assert(thread_to_destroy_id < concurrency);

			size_t thread_to_destroy_index = std::numeric_limits<size_t>::max();
			std::vector<std::thread> threads;

			std::mutex thread_destroyed_mutex;
			std::condition_variable thread_destroyed_cv;

			bool stop_threads = false;
			std::mutex stop_threads_mutex;
			std::condition_variable stop_threads_cv;

			syncthreads_barrier syncthreads(concurrency);
			for (size_t i = 0; i < concurrency; i++) {
				threads.emplace_back(
					[&](size_t index) {
						auto tid = thread_id_get(thread_id.get());

						/* Wait for all threads to call thread_id_get to make sure that none of
						 * the will reuse the id release by thread with 'id' ==
						 * 'thread_to_destroy'. */
						syncthreads();

						if (tid == thread_to_destroy_id) {
							/* Store it's index, notify main thread and wait for signal to
							 * finish. */
							std::unique_lock<std::mutex> lock(thread_destroyed_mutex);
							thread_to_destroy_index = index;
							thread_destroyed_cv.notify_one();
						} else {
							/* Run until notified. */
							std::unique_lock<std::mutex> lock(stop_threads_mutex);
							stop_threads_cv.wait(lock, [&] { return stop_threads; });
						}
					},
					i);
			}

			/* Wait for one thread to finish. */
			{
				std::unique_lock<std::mutex> lock(thread_destroyed_mutex);
				thread_destroyed_cv.wait(lock, [&] {
					return thread_to_destroy_index != std::numeric_limits<size_t>::max();
				});
				threads[thread_to_destroy_index].join();
			}

			/* This thread should reuse the released id. */
			std::thread new_thread(
				[&] { UT_ASSERTeq(thread_id_get(thread_id.get()), thread_to_destroy_id); });
			new_thread.join();

			/* Stop all remaining threads. */
			{
				std::unique_lock<std::mutex> lock(stop_threads_mutex);
				stop_threads = true;
				stop_threads_cv.notify_all();
			}

			for (size_t i = 0; i < threads.size(); i++) {
				if (threads[i].joinable())
					threads[i].join();
			}
		}
	});
}
