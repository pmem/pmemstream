// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_THREAD_HELPERS_HPP
#define LIBPMEMSTREAM_THREAD_HELPERS_HPP

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

static inline std::string get_msg_from_exception_ptr(std::exception_ptr ptr)
{
	try {
		std::rethrow_exception(ptr);
	} catch (std::exception &e) {
		return e.what();
	} catch (...) {
		return "Unknown exception";
	}
}

/* Prints number of active exceptions inside exception_ptrs and error message
 * concatenated from all of exception error messages.
 *
 * Rethrows first active exception from the vector.
 */
static inline void handle_exceptions(const std::vector<std::exception_ptr> &exception_ptrs)
{
	size_t exceptions_thrown_count = 0;
	std::string errormsg;
	for (auto &e : exception_ptrs) {
		if (e) {
			exceptions_thrown_count++;
			errormsg += get_msg_from_exception_ptr(e) + "\n";
		}
	}

	if (exceptions_thrown_count) {
		std::cerr << std::to_string(exceptions_thrown_count) + " exception(s) thrown! " << std::endl;
		std::cerr << errormsg << std::endl;

		for (auto &e : exception_ptrs) {
			if (e) {
				std::rethrow_exception(e);
			}
		}
	}
}

template <typename Function>
void parallel_exec(size_t threads_number, Function f)
{
	std::vector<std::thread> threads;
	threads.reserve(threads_number);

	std::vector<std::exception_ptr> exception_ptrs(threads_number);
	for (size_t i = 0; i < threads_number; ++i) {
		threads.emplace_back(
			[&](size_t id) {
				try {
					f(id);
				} catch (...) {
					exception_ptrs[id] = std::current_exception();
				}
			},
			i);
	}

	for (auto &t : threads) {
		t.join();
	}

	handle_exceptions(exception_ptrs);
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
