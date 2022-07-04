// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Common, internal utils */

#ifndef LIBPMEMSTREAM_UTIL_H
#define LIBPMEMSTREAM_UTIL_H

#include <stddef.h>
#include <stdint.h>

#include <stdio.h>
#ifdef PMEMSTREAM_USE_TSAN
#include <sanitizer/tsan_interface.h>
#endif

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define ALIGN_DOWN(size, align) ((size) & ~((align)-1))
#define IS_POW2(value) ((value != 0 && (value & (value - 1)) == 0))

#define MEMBER_SIZE(type, member) sizeof(((struct type *)NULL)->member)

/* XXX: add support for different architectures. */
#define CACHELINE_SIZE (64ULL)

#ifdef PMEMSTREAM_USE_TSAN
#define UTIL_TSAN_RELEASE(addr) __tsan_release(addr)
#define UTIL_TSAN_ACQUIRE(addr) __tsan_acquire(addr)
#else
#define UTIL_TSAN_RELEASE(addr)
#define UTIL_TSAN_ACQUIRE(addr)
#endif

/* atomic_store variants */
#define atomic_store_explicit_release(dst, val)                                                                        \
	do {                                                                                                           \
		UTIL_TSAN_RELEASE((void *)(dst));                                                                      \
		__atomic_store_n((dst), (val), __ATOMIC_RELEASE);                                                      \
	} while (0)

#define atomic_store_explicit_relaxed(dst, val)                                                                        \
	do {                                                                                                           \
		__atomic_store_n((dst), (val), __ATOMIC_RELAXED);                                                      \
	} while (0)

/* atomic_load variants */
#define atomic_load_acquire(src, ret)                                                                                  \
	do {                                                                                                           \
		__atomic_load((src), (ret), __ATOMIC_ACQUIRE);                                                         \
		UTIL_TSAN_ACQUIRE((void *)(src));                                                                      \
	} while (0)

#define atomic_load_relaxed(src, ret)                                                                                  \
	do {                                                                                                           \
		__atomic_load((src), (ret), __ATOMIC_RELAXED);                                                         \
	} while (0)

/* atomic_fetch_add variants */
#define atomic_fetch_add_relaxed(dst, value, ret)                                                                      \
	do {                                                                                                           \
		*ret = __atomic_fetch_add((dst), (value), __ATOMIC_RELAXED);                                           \
	} while (0)

#define atomic_fetch_add_release(dst, value, ret)                                                                      \
	do {                                                                                                           \
		UTIL_TSAN_RELEASE((void *)(dst));                                                                      \
		*ret = __atomic_fetch_add((dst), (value), __ATOMIC_RELEASE);                                           \
	} while (0)

#define atomic_fetch_add_acquire_release(dst, value, ret)                                                              \
	do {                                                                                                           \
		UTIL_TSAN_RELEASE((void *)(dst));                                                                      \
		*ret = __atomic_fetch_add((dst), (value), __ATOMIC_ACQ_REL);                                           \
		UTIL_TSAN_ACQUIRE((void *)(dst));                                                                      \
	} while (0)

#define atomic_add_relaxed(dst, value)                                                                                 \
	do {                                                                                                           \
		__atomic_fetch_add((dst), (value), __ATOMIC_RELAXED);                                                  \
	} while (0)

#define atomic_add_release(dst, value)                                                                                 \
	do {                                                                                                           \
		UTIL_TSAN_RELEASE((void *)(dst));                                                                      \
		__atomic_fetch_add((dst), (value), __ATOMIC_RELEASE);                                                  \
	} while (0)

#define atomic_add_acquire_release(dst, value)                                                                         \
	do {                                                                                                           \
		UTIL_TSAN_RELEASE((void *)(dst));                                                                      \
		__atomic_fetch_add((dst), (value), __ATOMIC_ACQ_REL);                                                  \
		UTIL_TSAN_ACQUIRE((void *)(dst));                                                                      \
	} while (0)

/* atomic_compare_exchange */
#define atomic_compare_exchange_acquire_release(dst, expected, desired, weak, ret)                                     \
	do {                                                                                                           \
		UTIL_TSAN_RELEASE((void *)(dst));                                                                      \
		*ret = __atomic_compare_exchange_n(dst, expected, desired, weak, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);  \
		UTIL_TSAN_ACQUIRE((void *)(dst));                                                                      \
	} while (0)

#endif /* LIBPMEMSTREAM_UTIL_H */
