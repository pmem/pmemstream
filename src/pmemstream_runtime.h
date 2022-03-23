// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_RUNTIME_H
#define LIBPMEMSTREAM_RUNTIME_H

#include <stdint.h>

#include <libpmem2.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pmemstream_runtime {
	void *base;

	pmem2_memcpy_fn memcpy;
	pmem2_memset_fn memset;
	pmem2_flush_fn flush;
	pmem2_drain_fn drain;
	pmem2_persist_fn persist;
};

static inline const uint8_t *pmemstream_offset_to_ptr(const struct pmemstream_runtime *data, uint64_t offset)
{
	return (const uint8_t *)data->base + offset;
}

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_RUNTIME_H */
