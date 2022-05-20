// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Common, internal utils */

#ifndef LIBPMEMSTREAM_UTIL_H
#define LIBPMEMSTREAM_UTIL_H

#include <stddef.h>
#include <stdint.h>

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define ALIGN_DOWN(size, align) ((size) & ~((align)-1))
#define IS_POW2(value) ((value != 0 && (value & (value - 1)) == 0))

#define MEMBER_SIZE(type, member) sizeof(((struct type *)NULL)->member)

/* XXX: add support for different architectures. */
#define CACHELINE_SIZE (64ULL)

#endif /* LIBPMEMSTREAM_UTIL_H */
