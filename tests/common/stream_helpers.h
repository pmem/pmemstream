// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_STREAM_HELPERS_H
#define LIBPMEMSTREAM_STREAM_HELPERS_H

#include "libpmemstream.h"
#include "unittest.h"

typedef struct pmemstream_test_env {
	struct pmem2_map *map;
	struct pmemstream *stream;
} pmemstream_test_env;

static inline pmemstream_test_env pmemstream_test_make_default(char *path)
{
	struct pmemstream_test_env env;
	env.map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	int ret = pmemstream_from_map(&env.stream, TEST_DEFAULT_BLOCK_SIZE, env.map);
	UT_ASSERTeq(ret, 0);

	return env;
}

static inline void pmemstream_test_teardown(struct pmemstream_test_env env)
{
	pmemstream_delete(&env.stream);
	pmem2_map_delete(&env.map);
}

#endif /* LIBPMEMSTREAM_STREAM_HELPERS_H */
