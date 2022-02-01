// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "mpmc_queue.hpp"

namespace
{
static constexpr unsigned max_concurrency = 128;
static constexpr size_t max_queue_size = UINT64_MAX - 1;

void verify_consume(mpmc_queue *mpmc_queue, size_t expected_size, size_t expected_offset)
{
	size_t ready_offset;
	auto consumed_size = mpmc_queue_consume(mpmc_queue, UINT64_MAX, &ready_offset);

	UT_ASSERTeq(consumed_size, expected_size);
	UT_ASSERTeq(ready_offset, expected_offset);
}

void mpsc_test_singlethread(const std::vector<uint32_t> &sizes_to_produce, const size_t num_producers)
{
	RC_PRE(sizes_to_produce.size() > 0);
	UT_ASSERT(num_producers > 0);

	auto queue = make_mpmc_queue(num_producers, max_queue_size);
	size_t sum_sizes_to_produce = std::accumulate(sizes_to_produce.begin(), sizes_to_produce.end(), 0ULL);
	auto divided_data = random_divide_data(sizes_to_produce, num_producers);

	size_t total_acquired_size = 0;
	size_t ready_to_consume = 0;

	/* Simulate multiple producers and consumers in a single thread.
	 * Iterate over divided_data and for each producer, call acquire().
	 *
	 * Then, call produce() for each producer but use different order
	 * than for acquire(). After each produce(), create a copy of mpmc_queue
	 * (snapshot) and check if consume returns proper offset and size.
	 */
	for (size_t i = 0; i < sizes_to_produce.size(); i++) {
		/* Acquire step. */
		size_t acquired_size = 0;
		for (size_t producer = 0; producer < num_producers; producer++) {
			if (i >= divided_data[producer].size())
				continue;
			auto size = divided_data[producer][i];
			mpmc_queue_acquire(queue.get(), producer, size);
			acquired_size += size;
		}
		total_acquired_size += acquired_size;

		/* Produce step. */
		auto produce_order = *rc::gen::unique<std::vector<size_t>>(num_producers,
									   rc::gen::inRange<size_t>(0, num_producers));

		/* Maps producer id to acquired_size. */
		auto produces_in_progress = std::map<size_t, size_t>{};
		for (auto producer : produce_order) {
			if (i < divided_data[producer].size()) {
				produces_in_progress[producer] = divided_data[producer][i];
			}
		}

		for (auto producer : produce_order) {
			auto snapshot = make_mpmc_queue_snapshot(queue.get());
			verify_consume(snapshot.get(), ready_to_consume, 0);

			if (i >= divided_data[producer].size()) {
				produces_in_progress.erase(producer);
				continue;
			}
			mpmc_queue_produce(queue.get(), producer);

			auto producer_it = produces_in_progress.find(producer);
			if (producer_it == produces_in_progress.begin()) {
				/* We called produced for producer with no dependencies. */
				ready_to_consume += producer_it->second;
				produces_in_progress.erase(producer);
			} else {
				UT_ASSERT(produces_in_progress.size());
				/* Once the dependency of current producer finishes, the data produced by this one will
				 * be ready for consume. */
				auto acquired_size = producer_it->second;
				--producer_it;
				producer_it->second += acquired_size;
				produces_in_progress.erase(producer);
			}
		}

		UT_ASSERTeq(produces_in_progress.size(), 0);
		UT_ASSERTeq(total_acquired_size, ready_to_consume);

		auto snapshot = make_mpmc_queue_snapshot(queue.get());
		verify_consume(snapshot.get(), ready_to_consume, 0);
	}

	UT_ASSERTeq(total_acquired_size, ready_to_consume);
	UT_ASSERTeq(sum_sizes_to_produce, ready_to_consume);
}

void mpsc_test(const std::vector<uint32_t> &sizes_to_produce)
{
	unsigned num_producers = *rc::gen::inRange<unsigned>(1, max_concurrency - 1);
	mpsc_test_singlethread(sizes_to_produce, num_producers);
}

} // namespace

int main(int argc, char *argv[])
{
	return run_test([&] {
		return_check ret;
	
		/* verify if empty queue cannot be consumed */
		{
			auto queue = make_mpmc_queue(1ULL, max_queue_size);

			uint64_t ready_offset;
			auto consumed_size = mpmc_queue_consume(queue.get(), UINT64_MAX, &ready_offset);

			UT_ASSERTeq(ready_offset, 0);
			UT_ASSERTeq(consumed_size, 0);
		}

		/* verify if queue cannot be consumed after acquire */
		{
			auto queue = make_mpmc_queue(1ULL, max_queue_size);

			mpmc_queue_acquire(queue.get(), 0, 1);

			uint64_t ready_offset;
			auto consumed_size = mpmc_queue_consume(queue.get(), UINT64_MAX, &ready_offset);

			UT_ASSERTeq(ready_offset, 0);
			UT_ASSERTeq(consumed_size, 0);
		}

		/* verify if queue of size UINT64_MAX cannot be created. */
		{
			try {
				auto queue = make_mpmc_queue(1ULL, UINT64_MAX);
				UT_ASSERT_UNREACHABLE;
			} catch (...) {
			}
		}

		ret += rc::check("verify if producer can consume it's own products",
				 [](const std::vector<uint32_t> &sizes_to_produce) {
					 auto queue = make_mpmc_queue(1ULL, max_queue_size);

					 uint64_t offset = 0;
					 uint64_t validation_offset = 0;
					 for (auto size : sizes_to_produce) {
						 offset = mpmc_queue_acquire(queue.get(), 0, size);
						 UT_ASSERT(offset == validation_offset);
						 validation_offset += size;

						 /* Consume before calling produce should fail. */
						 uint64_t ready_offset;
						 size_t consumed_size =
							 mpmc_queue_consume(queue.get(), UINT64_MAX, &ready_offset);
						 UT_ASSERTeq(consumed_size, 0);

						 mpmc_queue_produce(queue.get(), 0);

						 /* Now, it should succeed. */
						 consumed_size =
							 mpmc_queue_consume(queue.get(), UINT64_MAX, &ready_offset);
						 UT_ASSERT(consumed_size == size);
						 UT_ASSERT(ready_offset == offset);

						 UT_ASSERT(mpmc_queue_get_consumed_offset(queue.get()) ==
							   ready_offset + consumed_size);
					 }
				 });

		ret += rc::check("verify if single consumer, multiple producers scenario works", mpsc_test);

		ret += rc::check("verify if size checking works correctly", [](size_t queue_size, size_t acquire_size) {
			RC_PRE(queue_size > 0 && queue_size <= max_queue_size);

			auto queue = make_mpmc_queue(1ULL, queue_size);
			auto offset = mpmc_queue_acquire(queue.get(), 0, acquire_size);

			if (acquire_size > queue_size) {
				UT_ASSERTeq(offset, MPMC_QUEUE_OFFSET_MAX);
			} else {
				UT_ASSERTeq(offset, 0);
			}
		});
	});
}
