// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "mpmc_queue.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct mpmc_queue_producer {
	uint64_t granted_offset;
	uint64_t size;

	/* avoid false sharing by padding the variable */
	// XXX: calculate 6 from CACHELINE_SIZE
	uint64_t padding[6];
};

struct mpmc_queue {
	uint64_t num_producers;
	size_t size;
	uint64_t produce_offset;
	uint64_t padding_produce_offset[5];

	uint64_t consume_offset;
	uint64_t padding_consume_offset[7];

	struct mpmc_queue_producer producers[];
};

/* XXX: add support for dynamic producer registration? */
struct mpmc_queue *mpmc_queue_new(size_t num_producers, size_t size)
{
	struct mpmc_queue *manager = malloc(sizeof(*manager) + num_producers * sizeof(struct mpmc_queue_producer));
	if (!manager) {
		return NULL;
	}

	manager->size = size;
	manager->num_producers = num_producers;
	mpmc_queue_reset(manager, 0);

	return manager;
}

void mpmc_queue_destroy(struct mpmc_queue *manager)
{
	free(manager);
}

uint64_t mpmc_queue_acquire(struct mpmc_queue *manager, uint64_t producer_id, size_t size)
{
	assert(producer_id < manager->num_producers);

	struct mpmc_queue_producer *producer = &manager->producers[producer_id];
	uint64_t grant_offset = __atomic_load_n(&manager->produce_offset, __ATOMIC_RELAXED);

	if (grant_offset + size > manager->size) {
		return MPMC_QUEUE_OFFSET_MAX;
	}

	bool success = false;
	do {
		__atomic_store_n(&producer->granted_offset, grant_offset, __ATOMIC_RELAXED);
		const bool weak = true;
		success = __atomic_compare_exchange_n(&manager->produce_offset, &grant_offset, grant_offset + size,
						      weak, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	} while (!success);

	return grant_offset;
}

void mpmc_queue_produce(struct mpmc_queue *manager, uint64_t producer_id)
{
	assert(producer_id < manager->num_producers);

	struct mpmc_queue_producer *producer = &manager->producers[producer_id];
	__atomic_store_n(&producer->granted_offset, MPMC_QUEUE_OFFSET_MAX, __ATOMIC_RELEASE);
}

uint64_t mpmc_queue_get_consumed_offset(struct mpmc_queue *manager)
{
	return __atomic_load_n(&manager->consume_offset, __ATOMIC_ACQUIRE);
}

static uint64_t mpmc_queue_update_consumed_offset(struct mpmc_queue *manager, uint64_t consume_offset_snapshot,
						  uint64_t desired_offset, uint64_t *ready_offset)
{
	if (desired_offset <= consume_offset_snapshot) {
		*ready_offset = consume_offset_snapshot;
		return 0;
	}

	bool weak = false;
	bool success = __atomic_compare_exchange_n(&manager->consume_offset, &consume_offset_snapshot, desired_offset,
						   weak, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
	*ready_offset = consume_offset_snapshot;
	if (success) {
		return desired_offset - consume_offset_snapshot;
	} else {
		return 0;
	}
}

uint64_t mpmc_queue_consume_hint(struct mpmc_queue *manager, uint64_t producer_id, uint64_t max_producer_id,
				 size_t *ready_offset)
{
	if (producer_id >= manager->num_producers) {
		return mpmc_queue_consume(manager, max_producer_id, ready_offset);
	}

	uint64_t consume_offset = mpmc_queue_get_consumed_offset(manager);
	uint64_t granted_offset = manager->producers[producer_id].granted_offset;
	uint64_t produced_offset = granted_offset + manager->producers[producer_id].size;
	if (consume_offset == granted_offset) {
		/* producer_id was the first producer after last consume. */
		return mpmc_queue_update_consumed_offset(manager, consume_offset, produced_offset, ready_offset);
	}

	return mpmc_queue_consume(manager, max_producer_id, ready_offset);
}

uint64_t mpmc_queue_consume(struct mpmc_queue *manager, uint64_t max_producer_id, size_t *ready_offset)
{
	assert(manager->num_producers);

	/* produce_offset must be loaded before checking granted_offsets. */
	uint64_t produce_offset = __atomic_load_n(&manager->produce_offset, __ATOMIC_RELAXED);
	uint64_t min_granted_offset = MPMC_QUEUE_OFFSET_MAX;

	uint64_t max_id_to_check =
		(manager->num_producers - 1) < max_producer_id ? (manager->num_producers - 1) : max_producer_id;
	for (unsigned i = 0; i <= max_id_to_check; i++) {
		uint64_t granted_offset = __atomic_load_n(&manager->producers[i].granted_offset, __ATOMIC_RELAXED);
		if (granted_offset < min_granted_offset) {
			min_granted_offset = granted_offset;
		}
	}

	/* All producers have commited. */
	if (min_granted_offset == MPMC_QUEUE_OFFSET_MAX) {
		min_granted_offset = produce_offset;
	}

	return mpmc_queue_update_consumed_offset(manager, mpmc_queue_get_consumed_offset(manager), min_granted_offset,
						 ready_offset);
}

void mpmc_queue_reset(struct mpmc_queue *manager, uint64_t offset)
{
	manager->produce_offset = offset;
	manager->consume_offset = offset;

	for (unsigned i = 0; i < manager->num_producers; i++) {
		manager->producers[i].granted_offset = MPMC_QUEUE_OFFSET_MAX;
	}
}
