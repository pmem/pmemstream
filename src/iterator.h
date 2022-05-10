// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_ITERATOR_H
#define LIBPMEMSTREAM_ITERATOR_H

#include "libpmemstream.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pmemstream_entry_iterator {
	bool perform_recovery;
	struct pmemstream *const stream;
	const struct pmemstream_region region;
	struct pmemstream_region_runtime *const region_runtime;
	uint64_t offset;
};

struct pmemstream_region_iterator {
	struct pmemstream *const stream;
	struct pmemstream_region region;
	struct pmemstream_region prev_region;
};

/* Initializes pmemstream_entry_iterator pointed to by 'iterator'. 'perform_recovery' specifies whether this iterator
 * should perform region recovery when last valid entry is found. */
int entry_iterator_initialize(struct pmemstream_entry_iterator *iterator, struct pmemstream *stream,
			      struct pmemstream_region region, bool perform_recovery);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_ITERATOR_H */
