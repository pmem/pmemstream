// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Common, internal utils */

#ifndef LIBPMEMSTREAM_UTIL_H
#define LIBPMEMSTREAM_UTIL_H

#include "libpmemstream_internal.h"
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

int pmemstream_memcpy_impl(pmem2_memcpy_fn pmem2_memcpy, void *destination, const size_t argc, ...)
{
	/* Parse variadic arguments */
	va_list argv;
	const size_t parts = argc / 2;
	const size_t last_part = parts - 1;

	struct buffer {
		uint8_t *ptr;
		size_t size;
	};
	struct buffer parsed_args[parts];

	va_start(argv, argc);
	for (size_t i = 0; i < parts; i++) {
		parsed_args[i].ptr = va_arg(argv, uint8_t *);
		parsed_args[i].size = va_arg(argv, size_t);
	}
	va_end(argv);

	/* Do the memcpy */
	struct {
		uint8_t array[CACHELINE_SIZE];
		size_t offset;
	} tmp_buffer;
	tmp_buffer.offset = 0;

	uint8_t *dest = (uint8_t *)destination;

	for (size_t part = 0; part < parts; part++) {
		uint8_t *src = parsed_args[part].ptr;
		size_t size = parsed_args[part].size;

		struct buffer chunk;
		chunk.size = 0;

		size_t src_offset = 0;

		while (src_offset < size) {
			chunk.ptr = NULL;

			size_t tmp_buffer_free_space = sizeof(tmp_buffer.array) - tmp_buffer.offset;
			if (tmp_buffer_free_space >= size - src_offset) {
				chunk.size = size - src_offset;
			} else {
				chunk.size = tmp_buffer_free_space;
			}

			if (chunk.size == CACHELINE_SIZE) {
				chunk.ptr = src + src_offset;
			} else {
				memcpy(&tmp_buffer.array[tmp_buffer.offset], src + src_offset, chunk.size);
				chunk.ptr = tmp_buffer.array;
				tmp_buffer.offset += chunk.size;
			}

			if (tmp_buffer.offset == CACHELINE_SIZE || chunk.size == CACHELINE_SIZE) {
				pmem2_memcpy(dest, chunk.ptr, CACHELINE_SIZE, PMEM2_F_MEM_NONTEMPORAL);
				tmp_buffer.offset = 0;
				dest += CACHELINE_SIZE;
			} else if (part == last_part) {
				pmem2_memcpy(dest, chunk.ptr, chunk.size, PMEM2_F_MEM_NONTEMPORAL);
			}
			src_offset += chunk.size;
		}
	}

	return 0;
}

#define pmemstream_memcpy(pmem2_memcpy, dest, ...)                                                                     \
	pmemstream_memcpy_impl(pmem2_memcpy, dest, COUNT(__VA_ARGS__), __VA_ARGS__)

#endif /* LIBPMEMSTREAM_UTIL_H */
