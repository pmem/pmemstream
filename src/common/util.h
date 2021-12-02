// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Common, internal utils */

#ifndef LIBPMEMSTREAM_UTIL
#define LIBPMEMSTREAM_UTIL

#include <stddef.h>
#include <stdint.h>

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define ALIGN_DOWN(size, align) ((size) & ~((align)-1))

static inline unsigned char util_popcount64(uint64_t value)
{
	return (unsigned char)__builtin_popcountll(value);
}

static inline size_t util_popcount_memory(const uint8_t *data, size_t size)
{
	size_t count = 0;
	size_t i = 0;

	for (; i < ALIGN_DOWN(size, sizeof(uint64_t)); i += sizeof(uint64_t)) {
		count += util_popcount64(*(const uint64_t *)(data + i));
	}
	for (; i < size; i++) {
		count += util_popcount64(data[i]);
	}

	return count;
}

#endif /* LIBPMEMSTREAM_UTIL */
