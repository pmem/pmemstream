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
 */

enum region_runtime_state {
	REGION_RUNTIME_STATE_UNINITIALIZED, /* append_offset and committed_offset are invalid */
	REGION_RUNTIME_STATE_DIRTY, /* append_offset and committed_offsets are known but region was not yet cleared
		  (appending to such region might lead to data corruption) */
	REGION_RUNTIME_STATE_CLEAR  /* append_offset and committed_offset are known, append is safe */
};

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

enum region_runtime_state region_runtime_get_state(const struct pmemstream_region_runtime *region_runtime);

/* Precondition: region_runtime_get_state() != REGION_RUNTIME_STATE_UNINITIALIZED */
uint64_t region_runtime_get_append_offset_acquire(const struct pmemstream_region_runtime *region_runtime);
/* Precondition: region_runtime_get_state() != REGION_RUNTIME_STATE_UNINITIALIZED */
uint64_t region_runtime_get_committed_offset_acquire(const struct pmemstream_region_runtime *region_runtime);

/* Precondition: region_runtime_get_state() == REGION_RUNTIME_STATE_CLEAR */
void region_runtime_increase_append_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff);
/* Precondition: region_runtime_get_state() == REGION_RUNTIME_STATE_CLEAR */
void region_runtime_increase_committed_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff);

/*
 * Performs region recovery - initializes append_offset and committed_offset.
 * Also clears memory after append_offset.
 *
 * After this call, region_runtime is in "clear" state.
 */
int region_runtime_initialize_clear_locked(struct pmemstream *stream, struct pmemstream_region region,
					   struct pmemstream_region_runtime *region_runtime);

/*
 * Performs region recovery - initializes append_offset and committed_offset.
 *
 * After this call, region_runtime is in "dirty" state.
 */
void region_runtime_initialize_dirty_locked(struct pmemstream_region_runtime *region_runtime,
					    struct pmemstream_entry tail);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_REGION_H */
