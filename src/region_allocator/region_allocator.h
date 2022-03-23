// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_REGION_ALLOCATOR_H
#define LIBPMEMSTREAM_REGION_ALLOCATOR_H

#include "allocator_base.h"

#ifdef __cplusplus
extern "C" {
#endif

void allocator_runtime_initialize(const struct pmemstream_runtime *runtime, struct allocator_header *header);
uint64_t allocator_region_allocate(const struct pmemstream_runtime *runtime, struct allocator_header *header,
				   size_t size);
void allocator_region_free(const struct pmemstream_runtime *runtime, struct allocator_header *header, uint64_t offset);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif
