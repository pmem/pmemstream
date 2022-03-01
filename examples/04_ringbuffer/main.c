// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"
#include <stdbool.h>
#include <string.h>

#include <libpmem2.h>
#include <pthread.h>
#include <stdio.h>

#include "ringbuffer.h"

#define NUMBER_OF_REGIONS 10
#define REGION_SIZE 256

struct data_entry {
	uint64_t data;
};

/* XXX: Change to lazy creation */
int create_multiple_regions(struct pmemstream *stream, size_t number_of_regions, size_t region_size)
{
	for (size_t i = 0; i < number_of_regions; i++) {
		struct pmemstream_region new_region;
		int ret = pmemstream_region_allocate(stream, region_size, &new_region);
		if (ret != 0) {
			return ret;
		}
	}
	return 0;
}

void *producer_thread(void *args)
{
	struct ringbuffer_runtime *ringbuffer = (struct ringbuffer_runtime *)args;
	int i = 0;
	while (i < 100) {
		int ret = ringbuffer_runtime_produce(ringbuffer, &i, sizeof(i));
		if (ret == 0) {
			printf("prouced: %d\n ", i);
			i++;
		}
		sleep(0.01);
	}
	return NULL;
}

void *consumer_thread(void *args)
{
	struct ringbuffer_runtime *ringbuffer = (struct ringbuffer_runtime *)args;
	int consumed = 0xBEEF;
	int i = 0;
	while (i < 100) {
		int size = ringbuffer_runtime_consume(ringbuffer, &consumed);
		if (size > 0) {
			printf("consumed: %d\n", consumed);
			i++;
		}
	}
	return NULL;
}

void run_ringbuffer(struct pmemstream *stream)
{
	struct ringbuffer_runtime ringbuffer = ringbuffer_runtime_new(stream, REGION_SIZE);

	pthread_t producer;
	pthread_t consumer;

	pthread_create(&producer, NULL, producer_thread, &ringbuffer);
	pthread_create(&consumer, NULL, consumer_thread, &ringbuffer);

	pthread_join(producer, NULL);
	pthread_join(consumer, NULL);

	ringbuffer_runtime_delete(&ringbuffer);
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	struct pmem2_map *map = example_map_open(argv[1], 20 * EXAMPLE_STREAM_SIZE);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	if (ret == -1) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return ret;
	}

	/* Populate stream with regions */
	create_multiple_regions(stream, NUMBER_OF_REGIONS, REGION_SIZE);

	run_ringbuffer(stream);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
