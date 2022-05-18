// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_REGION_H
#define LIBPMEMSTREAM_REGION_H

#include "critnib/critnib.h"
#include "libpmemstream.h"
#include "pmemstream_runtime.h"
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

struct pmemstream_region_runtime;
struct region_runtimes_map;

struct region_runtimes_map *region_runtimes_map_new(struct pmemstream_runtime *data);
void region_runtimes_map_destroy(struct region_runtimes_map *map);

/* Gets (or creates if missing) pointer to region_runtime associated with specified region. */
int region_runtimes_map_get_or_create(struct region_runtimes_map *map, struct pmemstream_region region,
				      struct pmemstream_region_runtime **container_handle);
void region_runtimes_map_remove(struct region_runtimes_map *map, struct pmemstream_region region);

/* Precondition: region_runtime_iterate_and_initialize_for_write_locked must have been called. */
uint64_t region_runtime_get_append_offset_acquire(const struct pmemstream_region_runtime *region_runtime);

/* Precondition: region_runtime_iterate_and_initialize_for_write_locked must have been called. */
void region_runtime_increase_append_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff);

/*
 * Performs region recovery. This function iterates over entire region to find last entry and set append/committed
 * offset appropriately. * After this call, it's safe to write to the region. */
int region_runtime_iterate_and_initialize_for_write_locked(struct pmemstream *stream, struct pmemstream_region region,
							   struct pmemstream_region_runtime *region_runtime);

bool check_entry_consistency(const struct pmemstream_entry_iterator *iterator);

bool check_entry_and_maybe_recover_region(struct pmemstream_entry_iterator *iterator);

uint64_t region_first_entry_offset(struct pmemstream_region region);
#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_REGION_H */
