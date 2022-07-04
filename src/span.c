// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "span.h"

#include <assert.h>

#include "common/util.h"

struct span_base span_base_create(uint64_t size, enum span_type type)
{
	assert((size & SPAN_TYPE_MASK) == 0);
	struct span_base span = {.size_and_type = size | type};
	return span;
};

uint64_t span_get_size(const struct span_base *span)
{
	return span->size_and_type & SPAN_EXTRA_MASK;
}

size_t span_get_total_size(const struct span_base *span)
{
	size_t size = span_get_size(span);
	switch (span_get_type(span)) {
		case SPAN_EMPTY:
			size += sizeof(struct span_empty);
			break;
		case SPAN_ENTRY:
			size += sizeof(struct span_entry);
			break;
		case SPAN_REGION:
			size += sizeof(struct span_region);
			break;
		default:
			break;
	}

	/* Align so that each span starts at sizeof(span_bytes) aligned offset. */
	return ALIGN_UP(size, sizeof(span_bytes));
}

enum span_type span_get_type(const struct span_base *span)
{
	return span->size_and_type & SPAN_TYPE_MASK;
}

/* Following atomic store/load functions follow acquire/release semantics. */
void span_base_atomic_store(struct span_base *dst, struct span_base base)
{
	atomic_store_explicit_release(&dst->size_and_type, base.size_and_type);
}

void span_entry_atomic_store(struct span_entry *dst, struct span_entry entry)
{
	/* Store timestamp first because it's only valid for ENTRY span type. */
	atomic_store_explicit_relaxed(&dst->timestamp, entry.timestamp);
	atomic_store_explicit_release(&dst->span_base.size_and_type, entry.span_base.size_and_type);
}

struct span_entry span_entry_atomic_load(const struct span_entry *entry_ptr)
{
	struct span_entry entry;
	atomic_load_acquire(&entry_ptr->span_base.size_and_type, &entry.span_base.size_and_type);
	atomic_load_relaxed(&entry_ptr->timestamp, &entry.timestamp);
	return entry;
}
