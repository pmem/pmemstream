// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_ITERATOR_H
#define LIBPMEMSTREAM_ITERATOR_H

#include "libpmemstream.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*region_runtime_initialize_fn_type)(struct pmemstream_region_runtime *region_runtime, uint64_t offset);

struct pmemstream_entry_iterator {
	struct pmemstream *const stream;
	const struct pmemstream_region region;
	struct pmemstream_region_runtime *const region_runtime;
	uint64_t offset;

	region_runtime_initialize_fn_type region_runtime_initialize_fn;
};

struct pmemstream_region_iterator {
	struct pmemstream *const stream;
	struct pmemstream_region region;
};

/* Initializes pmemstream_entry_iterator pointed to by 'iterator'. */
int entry_iterator_initialize(struct pmemstream_entry_iterator *iterator, struct pmemstream *stream,
			      struct pmemstream_region region,
			      region_runtime_initialize_fn_type region_runtime_initialize_fn);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_ITERATOR_H */
