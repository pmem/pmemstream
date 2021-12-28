// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_ITERATOR_H
#define LIBPMEMSTREAM_ITERATOR_H

#include "libpmemstream.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pmemstream_entry_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
	struct pmemstream_region_context *region_context;
	size_t offset;
};

struct pmemstream_region_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
};

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream);
int pmemstream_region_iterator_next(struct pmemstream_region_iterator *it, struct pmemstream_region *region);
void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator);
// XXX: internal
int pmemstream_entry_iterator_initialize(struct pmemstream_entry_iterator *iterator, struct pmemstream *stream,
					 struct pmemstream_region region);
int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region);
/* Advances entry iterator by one. Verifies entry integrity and recovers the region if necessary. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter, struct pmemstream_region *region,
				   struct pmemstream_entry *user_entry);
void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_ITERATOR_H */
