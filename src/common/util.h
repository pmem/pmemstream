// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Common, internal utils */

#ifndef LIBPMEMSTREAM_UTIL_H
#define LIBPMEMSTREAM_UTIL_H

#include "libpmemstream_internal.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#if defined(__x86_64) || defined(_M_X64) || defined(__aarch64__) || defined(__riscv)
#define CACHELINE_SIZE 64ULL
#elif defined(__PPC64__)
#define CACHELINE_SIZE 128ULL
#else
#error unable to recognize architecture at compile time
#endif

/* macro for counting the number of varargs (up to 16)
 *  XXX: Should we extend this macro to 127 parameters? */
#define COUNT(...) COUNT_I(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define COUNT_I(_, _16, _15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, X, ...) X

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_UTIL_H */
