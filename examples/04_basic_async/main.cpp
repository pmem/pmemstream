// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <cstdio>
#include <libminiasync.h>
#include <libpmem2.h>

#define EXAMPLE_ASYNC_COUNT 3

struct data_entry {
	uint64_t data;
};

/**
 * Show example usage of sync and async appends.
 * Each async append is executed in a different region.
 */
int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	/* prepare stream and allocate or get a region */
	struct pmem2_map *map = example_map_open(argv[1], EXAMPLE_STREAM_SIZE);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	if (ret) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return ret;
	}

	/* get or allocate regions */
	struct pmemstream_region regions[EXAMPLE_ASYNC_COUNT];
	struct pmemstream_region_iterator *riter;
	ret = pmemstream_region_iterator_new(&riter, stream);
	if (ret) {
		fprintf(stderr, "pmemstream_region_iterator_new failed\n");
		return ret;
	}

	int i = 0;
	for (; i < EXAMPLE_ASYNC_COUNT; ++i) {
		ret = pmemstream_region_iterator_next(riter, &regions[i]);
		if (ret != 0) {
			break;
		}
	}
	/* if regions are missing - allocate them */
	for (; i < EXAMPLE_ASYNC_COUNT; ++i) {
		ret = pmemstream_region_allocate(stream, EXAMPLE_REGION_SIZE, &regions[i]);
		if (ret != 0) {
			fprintf(stderr, "pmemstream_region_allocate failed\n");
			return ret;
		}
	}
	pmemstream_region_iterator_delete(&riter);

	/* stream and regions are prepared, let's get to action */

	struct data_entry example_data[EXAMPLE_ASYNC_COUNT];
	example_data[0].data = 1;
	example_data[1].data = UINT64_MAX;
	example_data[2].data = 10000;
	struct pmemstream_entry entry;

	/*
	 * Example synchronous (regular) append
	 */
	ret = pmemstream_append(stream, regions[0], NULL, &example_data[0], sizeof(example_data[0]), &entry);
	if (ret) {
		fprintf(stderr, "pmemstream_append failed\n");
		return ret;
	}
	const struct data_entry *read_data = (const struct data_entry *)pmemstream_entry_data(stream, entry);
	printf("regular, synchronous append read data: %lu\n", read_data->data);

	/*
	 * Example asynchronous append, executed with libminiasync functions
	 */
	/* Prepare environment and define async appends (as futures) */
	struct data_mover_threads *dmt = data_mover_threads_default();
	if (dmt == NULL) {
		fprintf(stderr, "Failed to allocate data mover.\n");
		return -1;
	}
	struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);

	struct pmemstream_async_append_fut append_futures[EXAMPLE_ASYNC_COUNT];
	for (int i = 0; i < EXAMPLE_ASYNC_COUNT; ++i) {
		append_futures[i] = pmemstream_async_append(stream, thread_mover, regions[i], NULL, &example_data[i],
							    sizeof(example_data[i]));
	}

	/* Now, execute these futures. */

	/* For simply scenarios just run and wait for multiple futures: */
	// struct runtime *r = runtime_new();
	// runtime_wait_multiple(r, append_futures, EXAMPLE_ASYNC_COUNT);

	/* ... or alternatively manually poll until completion. */
	int completed_futures[EXAMPLE_ASYNC_COUNT] = {0};
	int completed = 0;
	do {
		for (int i = 0; i < EXAMPLE_ASYNC_COUNT; i++) {
			if (!completed_futures[i]) {
				auto fstate = future_poll(FUTURE_AS_RUNNABLE(&append_futures[i]), NULL);
				if (fstate == FUTURE_STATE_COMPLETE) {
					completed_futures[i] = 1;
					completed++;
					printf("Future %d is complete!\n", i);

					/* Since each append is done in an individual region, we can right away
					 * safely read out and print that appended value. */
					struct pmemstream_async_append_output *out = FUTURE_OUTPUT(&append_futures[i]);
					if (out->error_code != 0) {
						fprintf(stderr, "pmemstream_append_async (no. %d) failed\n", i);
						return out->error_code;
					}
					read_data = (const struct data_entry *)pmemstream_entry_data(stream,
												     out->new_entry);
					printf("async append (no. %d) read data: %lu\n", i, read_data->data);
				} else {
					/* this future is not completely done yet...
					 * but the entry may be already committed. */

					// uint8_t *data = (uint8_t
					// *)future_context_get_data(&append_futures[i].base.context); struct
					// future_chain_entry *entry = (struct future_chain_entry *)(data);
					// entry->future.task
					// .publish.fut.base.context.state == FUTURE_STATE_COMPLETE;
					printf("   We're trying!\n");

					struct future_chain_entry *memcpy_task =
						(struct future_chain_entry *)&append_futures[i].data.memcpy;
					if (append_futures[i].data.memcpy.init == NULL &&
					    FUTURE_CHAIN_ENTRY_IS_PROCESSED(memcpy_task)) {
						printf("   memcpy %i is now complete!\n", i);
					}

					struct future_chain_entry *publish_task =
						(struct future_chain_entry *)&append_futures[i].data.publish;
					if (append_futures[i].data.publish.init == NULL &&
					    FUTURE_CHAIN_ENTRY_IS_PROCESSED(publish_task)) {
						printf("   publish %i is now complete!\n", i);
					}

					struct future_chain_entry *persist_task =
						(struct future_chain_entry *)&append_futures[i].data.persist;
					if (append_futures[i].data.persist.init == NULL &&
					    FUTURE_CHAIN_ENTRY_IS_PROCESSED(persist_task)) {
						printf("   persist %i is now complete!\n", i);
					}

					// if (append_futures[i].data.memcpy.init == NULL &&
					// append_futures[i].data.memcpy.fut.base.context.state ==
					// FUTURE_STATE_COMPLETE) {
					// printf("   Publish %i is now FOR sure! complete!\n", i);
					// }
				}
			}
		}

		/*
		 * an additional user/application work could be done here
		 */
		printf("User work done here...\n");
	} while (completed < EXAMPLE_ASYNC_COUNT);

	/* cleanup */
	data_mover_threads_delete(dmt);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
