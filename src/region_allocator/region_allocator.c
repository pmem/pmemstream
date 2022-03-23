// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "region_allocator.h"
#include "libpmemstream_internal.h"

static void perform_free_list_extension(const struct pmemstream_runtime *runtime, struct allocator_header *header)
{
	struct span_region *span = (struct span_region *)span_offset_to_span_ptr(runtime, header->free_offset);

	SLIST_INSERT_HEAD(struct span_region, runtime, &header->free_list, header->free_offset,
			  allocator_metadata.next_free);

	header->free_offset += span_get_total_size(&span->span_base);
	runtime->persist(&header->free_offset, sizeof(header->free_offset));
}

static void recover_free_list_extension(const struct pmemstream_runtime *runtime, struct allocator_header *header)
{
	if (header->free_list.head != SLIST_INVALID_OFFSET && header->free_list.head > header->free_offset) {
		struct span_region *span =
			(struct span_region *)span_offset_to_span_ptr(runtime, header->free_list.head);

		header->free_offset += span_get_total_size(&span->span_base);
		runtime->persist(&header->free_offset, sizeof(header->free_offset));
	}
}

static void perform_free_list_head_to_allocated_list_tail_move(const struct pmemstream_runtime *runtime,
							       struct allocator_header *header)
{
	uint64_t region_free = header->free_list.head;

	struct span_base *span = (struct span_base *)span_offset_to_span_ptr(runtime, region_free);
	assert(span_get_type(span) == SPAN_REGION);

	/* XXX: remove after getting rid of popcount. */
	runtime->memset(((struct span_region *)span)->data, 0, span_get_size(span), PMEM2_F_MEM_NONTEMPORAL);

	SLIST_INSERT_TAIL(struct span_region, runtime, &header->allocated_list, region_free,
			  allocator_metadata.next_allocated);
	SLIST_REMOVE_HEAD(struct span_region, runtime, &header->free_list, allocator_metadata.next_free);
}

static void recover_free_list_head_to_allocated_list_tail_move(const struct pmemstream_runtime *runtime,
							       struct allocator_header *header)
{
	if (header->allocated_list.head != SLIST_INVALID_OFFSET &&
	    header->allocated_list.head == header->allocated_list.tail) {
		/* Continue allocation. */
		SLIST_REMOVE_HEAD(struct span_region, runtime, &header->free_list, allocator_metadata.next_free);
	}
}

static void perform_allocate_list_to_free_list_move(const struct pmemstream_runtime *runtime,
						    struct allocator_header *header, uint64_t offset)
{
	/* Store offset so we can redo the free on recovery */
	header->offset_to_free = offset;
	runtime->persist(&header->offset_to_free, sizeof(header->offset_to_free));

	SLIST_INSERT_HEAD(struct span_region, runtime, &header->free_list, offset, allocator_metadata.next_free);
	SLIST_REMOVE(struct span_region, runtime, &header->allocated_list, offset, allocator_metadata.next_allocated);

	header->offset_to_free = SLIST_INVALID_OFFSET;
	runtime->persist(&header->offset_to_free, sizeof(header->offset_to_free));
}

static void recover_allocate_list_to_free_list_move(const struct pmemstream_runtime *runtime,
						    struct allocator_header *header)
{
	if (header->offset_to_free == SLIST_INVALID_OFFSET)
		return;

	if (header->free_list.head != header->offset_to_free) {
		/* Crash just after setting header->offset_to_free */
		perform_allocate_list_to_free_list_move(runtime, header, header->free_list.head);
	} else {
		/* Crash after or before SLIST_REMOVE */

		// XXX: check if list contains the element, and only then call remove (or extend SLIST_REMOVE)
		SLIST_REMOVE(struct span_region, runtime, &header->allocated_list, header->offset_to_free,
			     allocator_metadata.next_allocated);

		header->offset_to_free = SLIST_INVALID_OFFSET;
		runtime->persist(&header->offset_to_free, sizeof(header->offset_to_free));
	}
}

static int extend_free_list(const struct pmemstream_runtime *runtime, struct allocator_header *header, uint64_t size)
{
	struct span_region span_region = {
		.span_base = span_base_create(size, SPAN_REGION),
		.allocator_metadata = {.next_allocated = SLIST_INVALID_OFFSET, .next_free = SLIST_INVALID_OFFSET}};

	if (span_get_total_size(&span_region.span_base) + header->free_offset > header->total_size) {
		return -1;
	}

	struct span_region *allocator_region_free_span =
		(struct span_region *)span_offset_to_span_ptr(runtime, header->free_offset);
	*allocator_region_free_span = span_region;
	runtime->persist(allocator_region_free_span, sizeof(*allocator_region_free_span));

	perform_free_list_extension(runtime, header);

	return 0;
}

void allocator_runtime_initialize(const struct pmemstream_runtime *runtime, struct allocator_header *header)
{
	SLIST_RUNTIME_INIT(struct span_region, runtime, &header->allocated_list, allocator_metadata.next_allocated);
	SLIST_RUNTIME_INIT(struct span_region, runtime, &header->free_list, allocator_metadata.next_free);

	recover_free_list_extension(runtime, header);
	recover_free_list_head_to_allocated_list_tail_move(runtime, header);
	recover_allocate_list_to_free_list_move(runtime, header);
}

uint64_t allocator_region_allocate(const struct pmemstream_runtime *runtime, struct allocator_header *header,
				   size_t size)
{
	uint64_t allocator_region_free = header->free_list.head;

	if (allocator_region_free == SLIST_INVALID_OFFSET) {
		int ret = extend_free_list(runtime, header, size);
		if (ret != 0) {
			return PMEMSTREAM_INVALID_OFFSET; // XXX: ENOMEM
		}
		allocator_region_free = header->free_list.head;
	}

	struct span_region *allocator_region_free_span =
		(struct span_region *)span_offset_to_span_ptr(runtime, allocator_region_free);
	assert(span_get_type(&allocator_region_free_span->span_base) == SPAN_REGION);
	assert(span_get_size(&allocator_region_free_span->span_base) == size);

	perform_free_list_head_to_allocated_list_tail_move(runtime, header);

	return allocator_region_free;
}

void allocator_region_free(const struct pmemstream_runtime *runtime, struct allocator_header *header, uint64_t offset)
{
	perform_allocate_list_to_free_list_move(runtime, header, offset);
}
