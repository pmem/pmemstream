// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_REGION_H
#define LIBPMEMSTREAM_REGION_H

#include "critnib/critnib.h"
#include "libpmemstream.h"
#include "span.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Functions for manipulating regions and region_runtime.
 *
 * Region_runtime can be in 3 different states:
 * - uninitialized: append_offset and commited_offset are invalid
 * - dirty: append_offset and commited_offsets are known but region was not yet cleared (appendind
 *   to such region might lead to data corruption)
 * - clear: append_offset and commited_offsets are known, append is safe
 */

struct pmemstream_region_runtime;
struct region_runtimes_map;

struct region_runtimes_map *region_runtimes_map_new(void);
void region_runtimes_map_destroy(struct region_runtimes_map *map);

/*
 * Gets (or creates if missing) pointer to region_runtime associated with specified region.
 *
 * Returned region_runtime might be in all 3 states (uninitialized, dirty or clear).
 */
int region_runtimes_map_get_or_create(struct region_runtimes_map *map, struct pmemstream_region region,
				      struct pmemstream_region_runtime **container_handle);
void region_runtimes_map_remove(struct region_runtimes_map *map, struct pmemstream_region region);

bool region_runtime_is_initialized(const struct pmemstream_region_runtime *region_runtime);
bool region_runtime_is_dirty(const struct pmemstream_region_runtime *region_runtime);

/* Precondition: region_runtime_is_initialized() == true */
uint64_t region_runtime_get_append_offset_acquire(const struct pmemstream_region_runtime *region_runtime);
/* Precondition: region_runtime_is_initialized() == true */
uint64_t region_runtime_get_commited_offset_acquire(const struct pmemstream_region_runtime *region_runtime);

void region_runtime_increase_append_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff);
void region_runtime_increase_commited_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff);

/* Recovers a region (under a global lock) if it is not yet recovered. */
int region_runtime_try_initialize_locked(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime *region_runtime);

/*
 * Performs region recovery - initializes append_offset and clears all the data in the region after `tail` entry.
 *
 * After this call, region_runtime is in "dirty" state.
 */
void region_runtime_initialize(struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry tail);

/*
 * Clears data in region starting from tail offset if region_runtime is in "dirty" state, does nothing otherwise.
 * After this call, region_runtime is in "clear" state.
 *
 * Returns current append_offset.
 */
uint64_t region_runtime_try_clear_from_tail(struct pmemstream *stream, struct pmemstream_region region,
					    struct pmemstream_region_runtime *region_runtime);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_REGION_H */
