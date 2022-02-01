// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <numeric>
#include <vector>

#include <rapidcheck.h>

#include "mpmc_queue.h"
#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
static constexpr unsigned max_concurrency = 1024;

auto make_mpmc_queue = make_instance_ctor(mpmc_queue_new, mpmc_queue_destroy);

// XXX: we could use vector of ranges here if they were available...
template <typename T>
std::vector<std::vector<T>> random_divide_data(const std::vector<T> &input, size_t into_num)
{
	using difference_type = typename std::vector<std::vector<T>>::difference_type;
	UT_ASSERT(into_num > 0);

	if (into_num == 1) {
		return {input};
	}

	std::vector<std::vector<T>> ret;
	difference_type divided_size = 0;
	for (size_t i = 0; i < into_num - 1; i++) {
		auto remaining_size = static_cast<difference_type>(input.size()) - divided_size;
		auto size = *rc::gen::inRange<difference_type>(0, remaining_size);
		ret.emplace_back(input.begin() + divided_size, input.begin() + divided_size + size);
		divided_size += size;
	}

	/* Last part is all what remains. */
	ret.emplace_back(input.begin() + divided_size, input.end());

	return ret;
}

// XXX - we should test with sizes vector bigger than 100 elements
void mpmc_test_generic(const std::vector<uint32_t> &sizes, const size_t num_producers, const size_t num_consumers)
{
	RC_PRE(sizes.size() > 0);
	UT_ASSERT(num_producers > 0);
	UT_ASSERT(num_consumers > 0);

	auto manager = make_mpmc_queue(num_producers, UINT64_MAX);
	size_t sum_sizes = std::accumulate(sizes.begin(), sizes.end(), 0ULL);

	struct consumer_state {
		struct range {
			uint64_t offset;
			uint64_t size;
		};

		std::vector<range> consumed;
	};

	std::vector<consumer_state> consumer_data(num_consumers);
	std::atomic<uint64_t> producer_end_offset = std::numeric_limits<uint64_t>::max();
	std::atomic<bool> all_consumed = false;

	auto divided_data = random_divide_data(sizes, num_producers);

	parallel_exec(num_consumers + num_producers, [&](size_t tid) {
		if (tid < num_producers) {
			/* producer */
			if (!divided_data[tid].size())
				return;

			uint64_t offset = 0;
			for (auto size : divided_data[tid]) {
				offset = mpmc_queue_acquire(manager.get(), tid, size);

				// XXX: *rc::gen::inRange<unsigned>(0, 10)?
				auto random_sleep_ms = std::chrono::milliseconds(3);
				std::this_thread::sleep_for(random_sleep_ms);

				mpmc_queue_produce(manager.get(), tid);
			}

			/* Store end offset. */
			uint64_t end_offset = offset + divided_data[tid].back();
			if (end_offset == sum_sizes) {
				producer_end_offset.store(end_offset, std::memory_order_relaxed);
			}
		} else {
			/* consumer */
			const size_t consumer_id = tid - num_producers;
			uint64_t consumed_offset = 0;
			while (!all_consumed.load(std::memory_order_relaxed)) {
				auto consumed_size = mpmc_queue_consume(manager.get(), UINT64_MAX, &consumed_offset);
				if (consumed_size) {
					consumer_data[consumer_id].consumed.push_back({consumed_offset, consumed_size});
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

	/* Merge all ranges from consumer_data and sort by offset. */
	auto all_consumed_ranges =
		std::accumulate(consumer_data.begin(), consumer_data.end(), std::vector<consumer_state::range>{},
				[](auto &&lhs, const auto &rhs) {
					lhs.insert(lhs.end(), rhs.consumed.begin(), rhs.consumed.end());
					return std::move(lhs);
				});
	std::sort(all_consumed_ranges.begin(), all_consumed_ranges.end(),
		  [](const consumer_state::range &lhs, const consumer_state::range &rhs) {
			  return lhs.offset < rhs.offset;
		  });

	/* One consumed range might cover multiple produced ranges. */
	UT_ASSERT(sizes.size() >= all_consumed_ranges.size());

	size_t offset = 0;
	for (auto r : all_consumed_ranges) {
		UT_ASSERT(r.offset == offset);
		offset += r.size;
	}

	UT_ASSERT(offset == sum_sizes);
}

void spmc_test(const std::vector<uint32_t> &sizes)
{
	unsigned num_consumers = *rc::gen::inRange<unsigned>(1, max_concurrency - 1);
	mpmc_test_generic(sizes, 1, num_consumers);
}

void mpsc_test(const std::vector<uint32_t> &sizes)
{
	unsigned num_producers = *rc::gen::inRange<unsigned>(1, max_concurrency - 1);
	mpmc_test_generic(sizes, num_producers, 1);
}

void mpmc_test(const std::vector<uint32_t> &sizes)
{
	unsigned num_consumers = *rc::gen::inRange<unsigned>(1, max_concurrency - 1);
	unsigned num_producers = *rc::gen::inRange<unsigned>(1, num_consumers);
	mpmc_test_generic(sizes, num_producers, num_consumers);
}

} // namespace

int main(int argc, char *argv[])
{
	return run_test([&] {
		return_check ret;

		// XXX: should we also test with uint64_t?

		ret += rc::check(
			"verify if producer can consume it's own products", [](const std::vector<uint32_t> &sizes) {
				auto manager = make_mpmc_queue(1ULL, UINT64_MAX);

				uint64_t offset = 0;
				uint64_t validation_offset = 0;
				for (auto size : sizes) {
					offset = mpmc_queue_acquire(manager.get(), 0, size);
					UT_ASSERT(offset == validation_offset);
					validation_offset += size;

					/* Consume before calling produce should fail. */
					uint64_t ready_offset;
					size_t consumed_size =
						mpmc_queue_consume(manager.get(), UINT64_MAX, &ready_offset);
					UT_ASSERTeq(consumed_size, 0);

					mpmc_queue_produce(manager.get(), 0);

					/* Now, it should succeed. */
					consumed_size = mpmc_queue_consume(manager.get(), UINT64_MAX, &ready_offset);
					UT_ASSERT(consumed_size == size);
					UT_ASSERT(ready_offset == offset);

					UT_ASSERT(mpmc_queue_get_consumed_offset(manager.get()) ==
						  ready_offset + consumed_size);
				}
			});

		// XXX: those tests should have shrinking disabled (they are multithreaded)!
		ret += rc::check("verify if multiple consumers, single producer scenario works", spmc_test);
		ret += rc::check("verify if single consumer, multiple producers scenario works", mpsc_test);
		ret += rc::check("verify if multiple consumers, multiple producers scenario works", mpmc_test);

		// XXX: add test with small queue size
	});
}
