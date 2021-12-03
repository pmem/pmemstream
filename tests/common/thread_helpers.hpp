// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

#ifndef LIBPMEMSTREAM_THREAD_HELPERS_HPP
#define LIBPMEMSTREAM_THREAD_HELPERS_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

template <typename Function>
void parallel_exec(size_t threads_number, Function f)
{
	std::vector<std::thread> threads;
	threads.reserve(threads_number);

	for (size_t i = 0; i < threads_number; ++i) {
		threads.emplace_back(f, i);
	}

	for (auto &t : threads) {
		t.join();
	}
}

class latch {
 public:
	latch(size_t desired) : counter(desired)
	{
	}

	/* Returns true for the last thread arriving at the latch, false for all
	 * other threads. */
	bool wait(std::unique_lock<std::mutex> &lock)
	{
		counter--;
		if (counter > 0) {
			cv.wait(lock, [&] { return counter == 0; });
			return false;
		} else {
			/*
			 * notify_call could be called outside of a lock
			 * (it would perform better) but drd complains
			 * in that case
			 */
			cv.notify_all();
			return true;
		}
	}

 private:
	std::condition_variable cv;
	size_t counter = 0;
};

/*
 * This function executes 'threads_number' threads and provides
 * 'syncthreads' method (multi-use synchronization barrier) for f()
 */
template <typename Function>
void parallel_xexec(size_t threads_number, Function f)
{
	std::mutex m;
	std::shared_ptr<latch> current_latch = std::shared_ptr<latch>(new latch(threads_number));

	/* Implements multi-use barrier (latch). Once all threads arrive at the
	 * latch, a new latch is allocated and used by all subsequent calls to
	 * syncthreads. */
	auto syncthreads = [&] {
		std::unique_lock<std::mutex> lock(m);
		auto l = current_latch;
		if (l->wait(lock))
			current_latch = std::shared_ptr<latch>(new latch(threads_number));
	};

	parallel_exec(threads_number, [&](size_t tid) { f(tid, syncthreads); });
}

/*
 * This function executes 'threads_number' threads and wait for all of them to
 * finish executing f before calling join().
 */
template <typename Function>
void parallel_exec_with_sync(size_t threads_number, Function f)
{
	parallel_xexec(threads_number, [&](size_t tid, std::function<void(void)> syncthreads) {
		f(tid);

		syncthreads();
	});
}

#endif /* LIBPMEMSTREAM_THREAD_HELPERS_HPP */
