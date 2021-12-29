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

/* Initializes entry_iterator pointed to by 'iterator'. */
int entry_iterator_initialize(struct pmemstream_entry_iterator *iterator, struct pmemstream *stream,
			      struct pmemstream_region region);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_ITERATOR_H */
