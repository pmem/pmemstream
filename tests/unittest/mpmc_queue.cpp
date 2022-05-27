// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <map>
#include <numeric>
#include <vector>

#include <rapidcheck.h>
#include <rapidcheck/state.h>

#include "mpmc_queue.h"
#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
auto make_mpmc_queue = make_instance_ctor(mpmc_queue_new, mpmc_queue_destroy);
auto make_mpmc_queue_snapshot = make_instance_ctor(mpmc_queue_copy, mpmc_queue_destroy);
using mpmc_queue_handle = decltype(make_mpmc_queue(0ULL, 0ULL));

static constexpr unsigned max_concurrency = 128;
static constexpr size_t max_queue_size = UINT64_MAX - 1;

void verify_consume(mpmc_queue *mpmc_queue, size_t expected_size, size_t expected_offset)
{
	size_t ready_offset;
	auto consumed_size = mpmc_queue_consume(mpmc_queue, UINT64_MAX, &ready_offset);

	UT_ASSERTeq(consumed_size, expected_size);
	UT_ASSERTeq(ready_offset, expected_offset);
}

struct producer {
	unsigned size;
	size_t id;
};

struct mpmc_model {
	// maps offsets to producers
	std::map<size_t, producer> acquired_but_not_produced;
	uint64_t acquired_offset = 0;
};

struct mpmc_sut {
	mpmc_sut() : queue(make_mpmc_queue(max_concurrency, max_queue_size))
	{
	}

	mpmc_sut(mpmc_sut &&) = delete;
	mpmc_sut(const mpmc_sut &) = delete;
	mpmc_sut &operator=(const mpmc_sut &) = delete;

	mpmc_queue_handle queue;
};

using mpmc_command = rc::state::Command<mpmc_model, mpmc_sut>;

struct acquire_command : public mpmc_command {
	size_t producer_id = *rc::gen::inRange<size_t>(0, max_concurrency);
	unsigned size = *rc::gen::arbitrary<unsigned>();

	void checkPreconditions(const mpmc_model &m) const override
	{
		for (auto &e : m.acquired_but_not_produced) {
			RC_PRE(e.second.id != producer_id);
		}

		RC_PRE(size > 0);
	}

	void apply(mpmc_model &m) const override
	{
		m.acquired_but_not_produced[m.acquired_offset] = producer{size, producer_id};
		m.acquired_offset += size;
	}

	void run(const mpmc_model &m, mpmc_sut &q) const override
	{
		mpmc_queue_acquire(q.queue.get(), producer_id, size);
	}

	void show(std::ostream &os) const override
	{
		os << "acquire(" << producer_id << " " << size << ")";
	}
};

struct produce_command : public mpmc_command {
	size_t producer_key;

	explicit produce_command(const mpmc_model &m)
	    : producer_key((*rc::gen::elementOf(m.acquired_but_not_produced)).first)
	{
	}

	void checkPreconditions(const mpmc_model &m) const override
	{
		RC_PRE(m.acquired_but_not_produced.size() > 0);
		RC_PRE(m.acquired_but_not_produced.find(producer_key) != m.acquired_but_not_produced.end());
	}

	void apply(mpmc_model &m) const override
	{
		m.acquired_but_not_produced.erase(producer_key);
	}

	void run(const mpmc_model &m, mpmc_sut &q) const override
	{
		size_t producer_id = m.acquired_but_not_produced.find(producer_key)->second.id;

		mpmc_queue_produce(q.queue.get(), producer_id);
		auto next_model = nextState(m);

		size_t size_ready_to_consume = 0;
		if (next_model.acquired_but_not_produced.size() == 0)
			size_ready_to_consume = next_model.acquired_offset;
		else
			size_ready_to_consume = next_model.acquired_but_not_produced.begin()->first;

		auto snapshot = mpmc_queue_copy(q.queue.get());
		verify_consume(snapshot, size_ready_to_consume, 0);
		mpmc_queue_destroy(snapshot);
	}

	void show(std::ostream &os) const override
	{
		os << "produce(" << producer_key << ")";
	}
};

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

		ret += rc::check("verify if producer can consume its own products",
				 [](const std::vector<uint32_t> &sizes_to_produce) {
					 auto queue = make_mpmc_queue(1ULL, max_queue_size);

					 uint64_t offset = 0;
					 uint64_t validation_offset = 0;
					 for (auto size : sizes_to_produce) {
						 offset = mpmc_queue_acquire(queue.get(), 0, size);
						 UT_ASSERTeq(offset, validation_offset);
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
						 UT_ASSERTeq(consumed_size, size);
						 UT_ASSERTeq(ready_offset, offset);

						 UT_ASSERTeq(mpmc_queue_get_consumed_offset(queue.get()),
							     ready_offset + consumed_size);
					 }
				 });

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

		ret += rc::check("verify acquire/produce sequence in single thread", []() {
			mpmc_model model;
			mpmc_sut queue;
			rc::state::check(model, queue,
					 rc::state::gen::execOneOfWithArgs<acquire_command, produce_command>());
		});
	});
}
