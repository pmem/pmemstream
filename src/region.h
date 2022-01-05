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
 * - By explicitly calling pmemstream_get_region_context() for the first time
 * - By calling pmemstream_append (only if region_context does not exist yet)
 * - By advancing an entry iterator past last entry in a region (only if region_context does not exist yet)
 */
struct pmemstream_region_context {
	/*
	 * Offset at which new entries will be appended. Can be set to PMEMSTREAM_OFFSET_UNINITIALIZED.
	 *
	 * If PMEMSTREAM_OFFSET_DIRTY_BIT is set, append_offset points to a valid location but the memory from
	 * 'append_offset & PMEMSTREAM_OFFSET_DIRTY_MASK' to the end of region was not yet cleared. */
	uint64_t append_offset;
};

/*
 * Holds mapping between region offset and region_context.
 */
struct region_contexts_map {
	critnib *container;
	pthread_mutex_t container_lock;
	pthread_mutex_t region_lock; /* XXX: for multiple regions, we might want to consider having more locks. */
};

struct region_contexts_map *region_contexts_map_new(void);
void region_contexts_map_destroy(struct region_contexts_map *map);

/* Gets (or creates if missing) pointer to region_context associated with specified region. */
int region_contexts_map_get_or_create(struct region_contexts_map *map, struct pmemstream_region region,
				      struct pmemstream_region_context **container_handle);

void region_contexts_map_remove(struct region_contexts_map *map, struct pmemstream_region region);

int region_is_append_offset_initialized(struct pmemstream_region_context *region_context);

/* Recovers a region (under a global lock) if it is not yet recovered. */
int region_try_initialize_append_offset_locked(struct pmemstream *stream, struct pmemstream_region region,
					       struct pmemstream_region_context *region_context);

/* Performs region recovery - initializes append_offset and clears all the data in the region after `tail` entry. */
void region_initialize_append_offset(struct pmemstream_region_context *region_context, struct pmemstream_entry tail);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_REGION_H */
