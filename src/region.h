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

/* After opening pmemstream, each region_runtime is in one of those 3 states.
 * The only possible state transitions are:
 * UNINITIALIZED -> READ_READY
 * UNINITIALIZED -> WRITE_READY
 * READ_READY -> WRITE_READY
 */
enum region_runtime_state {
	REGION_RUNTIME_STATE_UNINITIALIZED, /* region metadata is not initialized */
	REGION_RUNTIME_STATE_READ_READY,    /* reading from the region is safe */
	REGION_RUNTIME_STATE_WRITE_READY    /* reading and writing to the region is safe */
};

struct pmemstream_region_runtime;
struct region_runtimes_map;

struct region_runtimes_map *region_runtimes_map_new(void);
void region_runtimes_map_destroy(struct region_runtimes_map *map);

/*
 * Gets (or creates if missing) pointer to region_runtime associated with specified region.
 *
 * Returned region_runtime might be in either of 3 states (uninitialized, read_ready or write_ready).
 */
int region_runtimes_map_get_or_create(struct region_runtimes_map *map, struct pmemstream_region region,
				      struct pmemstream_region_runtime **container_handle);
void region_runtimes_map_remove(struct region_runtimes_map *map, struct pmemstream_region region);

enum region_runtime_state region_runtime_get_state_acquire(const struct pmemstream_region_runtime *region_runtime);

/* Precondition: region_runtime_get_state_acquire() != REGION_RUNTIME_STATE_UNINITIALIZED */
uint64_t region_runtime_get_append_offset_acquire(const struct pmemstream_region_runtime *region_runtime);
/* Precondition: region_runtime_get_state_acquire() != REGION_RUNTIME_STATE_UNINITIALIZED */
uint64_t region_runtime_get_committed_offset_acquire(const struct pmemstream_region_runtime *region_runtime);

/* Precondition: region_runtime_get_state_acquire() == REGION_RUNTIME_STATE_WRITE_READY */
void region_runtime_increase_append_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff);
/* Precondition: region_runtime_get_state_acquire() == REGION_RUNTIME_STATE_WRITE_READY */
void region_runtime_increase_committed_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff);

/*
 * Performs region recovery.
 * After this call, it's safe to write to the region.
 */
int region_runtime_initialize_for_write_locked(struct pmemstream *stream, struct pmemstream_region region,
					       struct pmemstream_region_runtime *region_runtime);

/*
 * Performs region recovery.
 * After this call, it's safe to read from the region.
 */
void region_runtime_initialize_for_read_locked(struct pmemstream_region_runtime *region_runtime,
					       struct pmemstream_entry tail);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_REGION_H */
