// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * Helper, common functions for examples.
 * They aim to simplify examples' workflow. */

#ifndef LIBPMEMSTREAM_EXAMPLES_HELPERS_H
#define LIBPMEMSTREAM_EXAMPLES_HELPERS_H

#include <fcntl.h>
#include <libpmem2.h>
#include <unistd.h>

static inline struct pmem2_map *example_map_open(const char *file)
{
	struct pmem2_source *source;
	struct pmem2_config *config;
	struct pmem2_map *map = NULL;

	int fd = open(file, O_RDWR);
	if (fd < 0)
		return NULL;

	if (pmem2_source_from_fd(&source, fd) != 0)
		goto err_fd;

	if (pmem2_config_new(&config) != 0)
		goto err_config;

	pmem2_config_set_required_store_granularity(config, PMEM2_GRANULARITY_PAGE);

	if (pmem2_map_new(&map, config, source) != 0)
		goto err_map;

err_map:
	pmem2_config_delete(&config);
err_config:
	pmem2_source_delete(&source);
err_fd:
	close(fd);

	return map;
}
#endif /* LIBPMEMSTREAM_EXAMPLES_HELPERS_H */
