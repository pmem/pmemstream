// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * Helper, common functions for examples.
 * They aim to simplify examples' workflow. */

#ifndef LIBPMEMSTREAM_EXAMPLES_HELPERS_H
#define LIBPMEMSTREAM_EXAMPLES_HELPERS_H

#include <fcntl.h>
#include <libpmem2.h>
#include <unistd.h>

/* example file size = 10MiB */
#define EXAMPLE_STREAM_SIZE (1024UL * 1024 * 10)
#define EXAMPLE_REGION_SIZE (1024UL * 10)

static inline struct pmem2_map *example_map_open(const char *file, const size_t size)
{
	const mode_t FILE_MODE = 0644;
	struct pmem2_source *source;
	struct pmem2_config *config;
	struct pmem2_map *map = NULL;

	/* open or create file */
	int fd = open(file, O_CREAT | O_RDWR, FILE_MODE);
	if (fd < 0)
		return NULL;

	/* if given size is non-zero we want to extend (most likely just created) file */
	if (size > 0) {
		if (ftruncate(fd, (off_t)size) != 0)
			goto err_fd;
	}

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
