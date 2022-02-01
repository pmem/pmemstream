// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <map>
#include <numeric>
#include <vector>

#include <rapidcheck.h>

#include "env_setter.hpp"
#include "mpmc_queue.h"
#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
auto make_mpmc_queue = make_instance_ctor(mpmc_queue_new, mpmc_queue_destroy);
auto make_mpmc_queue_snapshot = make_instance_ctor(mpmc_queue_copy, mpmc_queue_destroy);

static constexpr unsigned max_concurrency = 128;
static constexpr unsigned max_size = 128;
static constexpr size_t max_queue_size = UINT64_MAX - 1;

/* Holds vector of results from consume operations. */
struct consumer_state {
	struct range {
		uint64_t offset;
		uint64_t size;
	};

	std::vector<range> consumed;
};

size_t accumulate_values(const std::vector<std::vector<uint32_t>> &v)
{
	return std::accumulate(v.begin(), v.end(), 0ULL, [](size_t acc, const std::vector<uint32_t> &curr) {
		return std::accumulate(curr.begin(), curr.end(), acc);
	});
}

size_t accumulate_sizes(const std::vector<std::vector<uint32_t>> &v)
{
	return std::accumulate(v.begin(), v.end(), 0ULL,
			       [](size_t acc, const std::vector<uint32_t> &curr) { return acc + curr.size(); });
}

void verify_consumed_data(const std::vector<std::vector<uint32_t>> &sizes_to_produce,
			  const std::vector<consumer_state> &consumed_data)
{
	size_t sum_sizes_to_produce = accumulate_values(sizes_to_produce);
	size_t num_sizes_to_produce = accumulate_sizes(sizes_to_produce);

	/* Merge all ranges from consumed_data and sort by offset. */
	auto all_consumed_ranges =
		std::accumulate(consumed_data.begin(), consumed_data.end(), std::vector<consumer_state::range>{},
				[](auto &&lhs, const auto &rhs) {
					lhs.insert(lhs.end(), rhs.consumed.begin(), rhs.consumed.end());
					return std::move(lhs);
				});
	std::sort(all_consumed_ranges.begin(), all_consumed_ranges.end(),
		  [](const consumer_state::range &lhs, const consumer_state::range &rhs) {
			  return lhs.offset < rhs.offset;
		  });

	/* One consumed range might cover multiple produced ranges. */
	UT_ASSERT(num_sizes_to_produce >= all_consumed_ranges.size());

	size_t offset = 0;
	for (auto r : all_consumed_ranges) {
		UT_ASSERT(r.offset == offset);
		offset += r.size;
	}

	UT_ASSERT(offset == sum_sizes_to_produce);
}

void mpmc_test_generic(const std::vector<std::vector<uint32_t>> &sizes_to_produce, const size_t num_consumers)
{
	auto sleep_ms = std::chrono::milliseconds(*rc::gen::inRange<size_t>(0, 10));
	size_t num_producers = sizes_to_produce.size();

	RC_PRE(num_producers > 0 && num_producers + num_consumers <= max_concurrency);
	UT_ASSERT(num_consumers > 0);

	size_t sum_sizes_to_produce = accumulate_values(sizes_to_produce);
	RC_PRE(sum_sizes_to_produce > 0);

	auto queue = make_mpmc_queue(num_producers, max_queue_size);

	std::vector<consumer_state> consumed_data(num_consumers);
	std::atomic<uint64_t> producer_end_offset = std::numeric_limits<uint64_t>::max();
	std::atomic<bool> all_consumed = false;

	parallel_exec(num_consumers + num_producers, [&](size_t tid) {
		if (tid < num_producers) {
			/* producer */
			if (!sizes_to_produce[tid].size())
				return;

			uint64_t offset = 0;
			for (auto size : sizes_to_produce[tid]) {
				offset = mpmc_queue_acquire(queue.get(), tid, size);

				/* Delay calling produce so that we get a chance to test synchronization. */
				if (sleep_ms.count()) {
					std::this_thread::sleep_for(sleep_ms);
				}

				mpmc_queue_produce(queue.get(), tid);
			}

			/* Store end offset. */
			uint64_t end_offset = offset + sizes_to_produce[tid].back();
			if (end_offset == sum_sizes_to_produce) {
				producer_end_offset.store(end_offset, std::memory_order_relaxed);
			}
		} else {
			/* consumer */
			const size_t consumer_id = tid - num_producers;
			uint64_t consumed_offset = 0;
			while (!all_consumed.load(std::memory_order_relaxed)) {
				auto consumed_size = mpmc_queue_consume(queue.get(), UINT64_MAX, &consumed_offset);
				if (consumed_size) {
					consumed_data[consumer_id].consumed.push_back({consumed_offset, consumed_size});
				}

				if (consumed_offset + consumed_size ==
				    producer_end_offset.load(std::memory_order_relaxed)) {
					/* Last offset is ready - stop all consumers. */
					all_consumed.store(true, std::memory_order_relaxed);
					return;
				}
			}
		}
	});

	verify_consumed_data(sizes_to_produce, consumed_data);
}

void spmc_test(const std::vector<uint32_t> &sizes_to_produce)
{
	unsigned num_consumers = *rc::gen::inRange<unsigned>(1, max_concurrency - 1);
	mpmc_test_generic({sizes_to_produce}, num_consumers);
}

void mpsc_test(const std::vector<std::vector<uint32_t>> &sizes_to_produce)
{
	mpmc_test_generic(sizes_to_produce, 1);
}

void mpmc_test(const std::vector<std::vector<uint32_t>> &sizes_to_produce)
{
	unsigned num_consumers = *rc::gen::inRange<unsigned>(1, max_concurrency - 1);
	mpmc_test_generic(sizes_to_produce, num_consumers);
}

} // namespace

int main(int argc, char *argv[])
{
	return run_test([&] {
		return_check ret;

		std::string rapidcheck_config = "noshrink=1 max_size=" + std::to_string(max_size);
		env_setter setter("RC_PARAMS", rapidcheck_config, false);

		ret += rc::check("verify if multiple consumers, single producer scenario works", spmc_test);
		ret += rc::check("verify if single consumer, multiple producers scenario works", mpsc_test);
		ret += rc::check("verify if multiple consumers, multiple producers scenario works", mpmc_test);
	});
}
