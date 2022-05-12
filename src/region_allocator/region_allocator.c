// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "region_allocator.h"
#include "libpmemstream_internal.h"

static void perform_free_list_extension(const struct pmemstream_runtime *runtime, struct allocator_header *header)
{
	struct span_region *span = (struct span_region *)span_offset_to_span_ptr(runtime, header->free_offset);

	SLIST_INSERT_HEAD(struct span_region, runtime, &header->free_list, header->free_offset,
			  allocator_entry_metadata.next_free);

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

	((struct span_region *)span)->max_valid_timestamp = PMEMSTREAM_INVALID_TIMESTAMP;
	runtime->persist(&((struct span_region *)span)->max_valid_timestamp, sizeof(uint64_t));
	runtime->memset(((struct span_region *)span)->data, 0, sizeof(struct span_entry), PMEM2_F_MEM_NONTEMPORAL);

	SLIST_INSERT_TAIL(struct span_region, runtime, &header->allocated_list, region_free,
			  allocator_entry_metadata.next_allocated);
	SLIST_REMOVE_HEAD(struct span_region, runtime, &header->free_list, allocator_entry_metadata.next_free);
}

static void recover_free_list_head_to_allocated_list_tail_move(const struct pmemstream_runtime *runtime,
							       struct allocator_header *header)
{
	if (header->free_list.head != SLIST_INVALID_OFFSET && header->free_list.head == header->allocated_list.tail) {
		/* Crash after insert - continue with removal */
		SLIST_REMOVE_HEAD(struct span_region, runtime, &header->free_list, allocator_entry_metadata.next_free);
	}
}

static void perform_allocated_list_to_free_list_move(const struct pmemstream_runtime *runtime,
						     struct allocator_header *header, uint64_t offset)
{
	/* Store offset so we can redo the free on recovery */
	header->recovery_free_offset = offset;
	runtime->persist(&header->recovery_free_offset, sizeof(header->recovery_free_offset));

	SLIST_INSERT_HEAD(struct span_region, runtime, &header->free_list, offset, allocator_entry_metadata.next_free);
	SLIST_REMOVE(struct span_region, runtime, &header->allocated_list, offset,
		     allocator_entry_metadata.next_allocated);

	header->recovery_free_offset = SLIST_INVALID_OFFSET;
	runtime->persist(&header->recovery_free_offset, sizeof(header->recovery_free_offset));
}

static void recover_allocated_list_to_free_list_move(const struct pmemstream_runtime *runtime,
						     struct allocator_header *header)
{
	if (header->recovery_free_offset == SLIST_INVALID_OFFSET)
		return;

	if (header->free_list.head != header->recovery_free_offset) {
		/* Crash just after setting header->recovery_free_offset */
		perform_allocated_list_to_free_list_move(runtime, header, header->recovery_free_offset);
	} else {
		/* Crash after or before SLIST_REMOVE */

		SLIST_REMOVE(struct span_region, runtime, &header->allocated_list, header->recovery_free_offset,
			     allocator_entry_metadata.next_allocated);

		header->recovery_free_offset = SLIST_INVALID_OFFSET;
		runtime->persist(&header->recovery_free_offset, sizeof(header->recovery_free_offset));
	}
}

static int extend_free_list(const struct pmemstream_runtime *runtime, struct allocator_header *header, uint64_t size)
{
	struct span_region span_region = {.span_base = span_base_create(size, SPAN_REGION),
					  .allocator_entry_metadata = {.next_allocated = SLIST_INVALID_OFFSET,
								       .next_free = SLIST_INVALID_OFFSET}};

	if (span_get_total_size(&span_region.span_base) + header->free_offset > header->size) {
		return -1;
	}

	struct span_region *free_span = (struct span_region *)span_offset_to_span_ptr(runtime, header->free_offset);
	*free_span = span_region;
	runtime->persist(free_span, sizeof(*free_span));

	perform_free_list_extension(runtime, header);

	return 0;
}

void allocator_runtime_initialize(const struct pmemstream_runtime *runtime, struct allocator_header *header)
{
	SLIST_RUNTIME_INIT(struct span_region, runtime, &header->allocated_list,
			   allocator_entry_metadata.next_allocated);
	SLIST_RUNTIME_INIT(struct span_region, runtime, &header->free_list, allocator_entry_metadata.next_free);

	recover_free_list_extension(runtime, header);
	recover_free_list_head_to_allocated_list_tail_move(runtime, header);
	recover_allocated_list_to_free_list_move(runtime, header);
}

uint64_t allocator_region_allocate(const struct pmemstream_runtime *runtime, struct allocator_header *header,
				   size_t size)
{
	uint64_t free_region = header->free_list.head;

	if (free_region == SLIST_INVALID_OFFSET) {
		int ret = extend_free_list(runtime, header, size);
		if (ret != 0) {
			return PMEMSTREAM_INVALID_OFFSET; // XXX: ENOMEM
		}
		free_region = header->free_list.head;
	}

	assert(span_get_type(span_offset_to_span_ptr(runtime, free_region)) == SPAN_REGION);
	assert(span_get_size(span_offset_to_span_ptr(runtime, free_region)) == size);

	perform_free_list_head_to_allocated_list_tail_move(runtime, header);

	return free_region;
}

void allocator_region_free(const struct pmemstream_runtime *runtime, struct allocator_header *header, uint64_t offset)
{
	perform_allocated_list_to_free_list_move(runtime, header, offset);
}
