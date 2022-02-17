// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <cstdio>
#include <libminiasync.h>
#include <libpmem2.h>

struct data_entry {
	uint64_t data;
};

/**
 * ...
 */
int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

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

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, 4096, &region);
	if (ret == -1) {
		struct pmemstream_region_iterator *riter;
		ret = pmemstream_region_iterator_new(&riter, stream);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_region_iterator_new failed\n");
			return ret;
		}
		/* if allocate failed, we try to open existing region */
		ret = pmemstream_region_iterator_next(riter, &region);
		pmemstream_region_iterator_delete(&riter);

		if (ret == -1) {
			fprintf(stderr, "pmemstream_region_iterator_next found no regions\n");
			return ret;
		}
	}

	struct data_entry e;
	e.data = 1;
	struct pmemstream_entry entry;

	/* regular (sync) append */
	ret = pmemstream_append(stream, region, NULL, &e, sizeof(e), &entry);
	if (ret) {
		fprintf(stderr, "pmemstream_append failed\n");
		return ret;
	}

	/* async append, executed in the libminiasync runtime
	 * XXX: use wait_many */
	struct pmemstream_append_future append_async = pmemstream_append_async(stream, region, NULL, &e, sizeof(e), &entry);

	struct runtime *r = runtime_new();
	runtime_wait(r, FUTURE_AS_RUNNABLE(&append_async));
	runtime_delete(r);

	struct pmemstream_async_append_output *out = FUTURE_OUTPUT(&append_async);
	if (out->error_code != 0) {
		fprintf(stderr, "pmemstream_append_async failed\n");
		return ret;
	}

	const struct data_entry *read_data = (const struct data_entry *)pmemstream_entry_data(stream, out->new_entry);
	printf("read_data: %lu\n", read_data->data);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
