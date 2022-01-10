// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_REGION_H
#define LIBPMEMSTREAM_REGION_H

#include "critnib/critnib.h"
#include "libpmemstream.h"
#include "span.h"

#include <pthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Functions for manipulating regions.
 */

#define PMEMSTREAM_OFFSET_UNINITIALIZED 0ULL
#define PMEMSTREAM_OFFSET_DIRTY_BIT (1ULL << 63)
#define PMEMSTREAM_OFFSET_DIRTY_MASK (~PMEMSTREAM_OFFSET_DIRTY_BIT)

/*
 * It contains all runtime data specific to a region.
 * It is always managed by the pmemstream (user can only obtain a non-owning pointer) and can be created
 * in few different ways:
 * - By explicitly calling pmemstream_get_region_runtime() for the first time
 * - By calling pmemstream_append (only if region_runtime does not exist yet)
 * - By advancing an entry iterator past last entry in a region (only if region_runtime does not exist yet)
 */
struct pmemstream_region_runtime {
	/*
	 * Offset at which new entries will be appended. Can be set to PMEMSTREAM_OFFSET_UNINITIALIZED.
	 *
	 * If PMEMSTREAM_OFFSET_DIRTY_BIT is set, append_offset points to a valid location but the memory from
	 * 'append_offset & PMEMSTREAM_OFFSET_DIRTY_MASK' to the end of region was not yet cleared. */
	uint64_t append_offset;
};

/*
 * Holds mapping between region offset and region_runtime.
 */
struct region_runtime_map {
	critnib *container;
	pthread_mutex_t container_lock;
	pthread_mutex_t region_lock; /* XXX: for multiple regions, we might want to consider having more locks. */
};

struct region_runtime_map *region_runtime_map_new(void);
void region_runtime_map_destroy(struct region_runtime_map *map);

/* Gets (or creates if missing) pointer to region_runtime associated with specified region. */
int region_runtime_map_get_or_create(struct region_runtime_map *map, struct pmemstream_region region,
				     struct pmemstream_region_runtime **container_handle);

void region_runtime_map_remove(struct region_runtime_map *map, struct pmemstream_region region);

int region_is_runtime_initialized(struct pmemstream_region_runtime *region_runtime);

/* Recovers a region (under a global lock) if it is not yet recovered. */
int region_try_runtime_initialize_locked(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime *region_runtime);

/* Performs region recovery - initializes append_offset and clears all the data in the region after `tail` entry. */
void region_runtime_initialize(struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry tail);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_REGION_H */
