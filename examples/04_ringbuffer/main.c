// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"
#include <stdbool.h>
#include <string.h>

#include <libpmem2.h>
#include <stdio.h>
#include <stdbool.h>

#include "ringbuffer.h"


#define NUMBER_OF_REGIONS 10
#define REGION_SIZE 4096

struct data_entry {
	uint64_t data;
};

/* XXX: Change to lazy creation */
int create_multiple_regions(struct pmemstream *stream, size_t number_of_regions, size_t region_size){
	for(size_t i=0; i< number_of_regions; i++) {
		struct pmemstream_region new_region;
		int ret = pmemstream_region_allocate(stream, region_size, &new_region);
		if( ret != 0){
			return ret;
		}
	}
	return 0;
}

void run_ringbuffer(struct pmemstream *stream){
	struct ringbuffer_runtime ringbuffer = ringbuffer_runtime_new(stream, REGION_SIZE);

	int i = 1;
	while( ringbuffer_runtime_produce(&ringbuffer, &i, sizeof(i))) {
		i++;
	}

	int consumed;
	for(int i =0; i< 42; i++) {
		ringbuffer_runtime_consume(&ringbuffer, &consumed);
		printf("%d, ", consumed);
	}
	printf("\n");

	while(ringbuffer_runtime_produce(&ringbuffer, &i, sizeof(i))) {
		i++;
	}

	while(ringbuffer_runtime_consume(&ringbuffer, &consumed) != 0) {
		ringbuffer_runtime_consume(&ringbuffer, &consumed);
		printf("%d, ", consumed);
	}

	printf("\n");
	ringbuffer_runtime_delete(&ringbuffer);
}


int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("Usage: %s file number", argv[0]);
		return -1;
	}

	struct pmem2_map *map = example_map_open(argv[1], EXAMPLE_STREAM_SIZE);
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

