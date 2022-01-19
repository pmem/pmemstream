// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "memcpy.h"
#include "common/util.h"

#include <assert.h>
#include <string.h>

void *pmemstream_memcpy(pmem2_memcpy_fn pmem2_memcpy, void *destination, const void *source, size_t count)
{
	uint8_t *dest = (uint8_t *)destination;
	uint8_t *src = (uint8_t *)source;

	const size_t dest_missalignment = ALIGN_UP((size_t)dest, CACHELINE_SIZE) - (size_t)dest;

	/* Align destination with cache line */
	if (dest_missalignment > 0) {
		size_t to_align = (dest_missalignment > count) ? count : dest_missalignment;
		pmem2_memcpy(dest, src, to_align, 0);
		if (count <= dest_missalignment) {
			return destination;
		}
		dest += to_align;
		src += to_align;
		count -= to_align;
	}

	assert((size_t)dest % CACHELINE_SIZE == 0);

	size_t not_aligned_tail_size = count % CACHELINE_SIZE;
	size_t alligned_size = count - not_aligned_tail_size;

	assert(alligned_size % CACHELINE_SIZE == 0);

	/* Copy all the data which may be alligned to the cache line */
	if (alligned_size != 0) {
		unsigned flags = PMEM2_F_MEM_NONTEMPORAL;
		/* NODRAIN if another pmem2_memcpy would be called */
		if (not_aligned_tail_size != 0) {
			flags |= PMEM2_F_MEM_NODRAIN;
		}
		pmem2_memcpy(dest, src, alligned_size, flags);
		dest += alligned_size;
		src += alligned_size;
	}

	/* Copy rest of the data */

	if (not_aligned_tail_size != 0) {
		pmem2_memcpy(dest, src, not_aligned_tail_size, PMEM2_F_MEM_NONTEMPORAL);
	}
	return destination;
}
