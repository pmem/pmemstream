// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "mpmc_queue.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct mpmc_queue_producer {
	uint64_t granted_offset;

	/* avoid false sharing by padding the variable */
	// XXX: calculate 7 from CACHELINE_SIZE
	uint64_t padding[7];
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
	if (size == UINT64_MAX) {
		return NULL;
	}

	struct mpmc_queue *queue = malloc(sizeof(*queue) + num_producers * sizeof(struct mpmc_queue_producer));
	if (!queue) {
		return NULL;
	}

	queue->size = size;
	queue->num_producers = num_producers;
	mpmc_queue_reset(queue, 0);

	return queue;
}

struct mpmc_queue *mpmc_queue_copy(struct mpmc_queue *queue)
{
	struct mpmc_queue *new_queue = mpmc_queue_new(queue->num_producers, queue->size);
	if (new_queue == NULL) {
		return NULL;
	}

	*new_queue = *queue;
	for (size_t i = 0; i < new_queue->num_producers; i++) {
		new_queue->producers[i] = queue->producers[i];
	}

	return new_queue;
}

void mpmc_queue_destroy(struct mpmc_queue *queue)
{
	free(queue);
}

uint64_t mpmc_queue_acquire(struct mpmc_queue *queue, uint64_t producer_id, size_t size)
{
	struct mpmc_queue_producer *producer = &queue->producers[producer_id];
	uint64_t grant_offset = __atomic_load_n(&queue->produce_offset, __ATOMIC_RELAXED);

	bool success = false;
	do {
		if (grant_offset + size > queue->size) {
			return MPMC_QUEUE_OFFSET_MAX;
		}

		__atomic_store_n(&producer->granted_offset, grant_offset, __ATOMIC_RELAXED);
		const bool weak = true;
		success = __atomic_compare_exchange_n(&queue->produce_offset, &grant_offset, grant_offset + size, weak,
						      __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	} while (!success);

	return grant_offset;
}

void mpmc_queue_produce(struct mpmc_queue *queue, uint64_t producer_id)
{
	struct mpmc_queue_producer *producer = &queue->producers[producer_id];
	__atomic_store_n(&producer->granted_offset, MPMC_QUEUE_OFFSET_MAX, __ATOMIC_RELEASE);
}

uint64_t mpmc_queue_get_consumed_offset(struct mpmc_queue *queue)
{
	return __atomic_load_n(&queue->consume_offset, __ATOMIC_ACQUIRE);
}

uint64_t mpmc_queue_consume(struct mpmc_queue *queue, uint64_t max_producer_id, size_t *ready_offset)
{
	/* produce_offset must be loaded before checking granted_offsets. */
	uint64_t produce_offset = __atomic_load_n(&queue->produce_offset, __ATOMIC_RELAXED);
	/* We can only consume offsets up to min_granted_offset. */
	uint64_t min_granted_offset = MPMC_QUEUE_OFFSET_MAX;
	assert(queue->num_producers);
	uint64_t max_id_to_check =
		(queue->num_producers - 1) < max_producer_id ? (queue->num_producers - 1) : max_producer_id;
	for (unsigned i = 0; i <= max_id_to_check; i++) {
		uint64_t granted_offset = __atomic_load_n(&queue->producers[i].granted_offset, __ATOMIC_RELAXED);
		if (granted_offset < min_granted_offset) {
			min_granted_offset = granted_offset;
		}
	}

	/* All producers have commited. */
	if (min_granted_offset == MPMC_QUEUE_OFFSET_MAX) {
		min_granted_offset = produce_offset;
	}

	uint64_t consume_offset = mpmc_queue_get_consumed_offset(queue);
	if (consume_offset < min_granted_offset) {
		bool weak = false;
		bool success = __atomic_compare_exchange_n(&queue->consume_offset, &consume_offset, min_granted_offset,
							   weak, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
		if (success) {
			*ready_offset = consume_offset;
			return min_granted_offset - consume_offset;
		}
	}

	*ready_offset = consume_offset;
	return 0;
}

void mpmc_queue_reset(struct mpmc_queue *queue, uint64_t offset)
{
	queue->produce_offset = offset;
	queue->consume_offset = offset;

	for (unsigned i = 0; i < queue->num_producers; i++) {
		queue->producers[i].granted_offset = MPMC_QUEUE_OFFSET_MAX;
	}
}
