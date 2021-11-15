// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "libpmemstream.h"
#include "ravl.h"

#include <assert.h>
#include <libpmem2.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define ALIGN_DOWN(size, align) ((size) & ~((align)-1))

enum pmemstream_span_type {
	PMEMSTREAM_SPAN_FREE = 00ULL << 62,
	PMEMSTREAM_SPAN_REGION = 11ULL << 62,
	PMEMSTREAM_SPAN_TX = 10ULL << 62,
	PMEMSTREAM_SPAN_ENTRY = 01ULL << 62,
};

#define PMEMSTREAM_SPAN_TYPE_MASK (11ULL << 62)
#define PMEMSTREAM_SPAN_EXTRA_MASK (~PMEMSTREAM_SPAN_TYPE_MASK)

struct pmemstream_span {
	uint64_t type_extra;
	uint64_t data[];
};

struct pmemstream_span_runtime {
	enum pmemstream_span_type type;
	size_t total_size;
	uint8_t *data;
	union {
		struct {
			uint64_t size;
		} free;
		struct {
			uint64_t txid;
			uint64_t size;
		} region;
		struct {
			uint64_t size;
		} entry;
		struct {
			uint64_t txid;
		} tx;
	};
};

static void
pmemstream_span_create_free(struct pmemstream_span *span, size_t data_size)
{
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span->type_extra = data_size | PMEMSTREAM_SPAN_FREE;
}

static void
pmemstream_span_create_entry(struct pmemstream_span *span, size_t data_size)
{
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span->type_extra = data_size | PMEMSTREAM_SPAN_ENTRY;
}

static void
pmemstream_span_create_region(struct pmemstream_span *span, size_t txid,
			      size_t size)
{
	assert((txid & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span->type_extra = txid | PMEMSTREAM_SPAN_REGION;
	span->data[0] = size;
}

static void
pmemstream_span_create_tx(struct pmemstream_span *span, size_t txid)
{
	assert((txid & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span->type_extra = txid | PMEMSTREAM_SPAN_TX;
}

static struct pmemstream_span_runtime
pmemstream_span_get_runtime(struct pmemstream_span *span)
{
	struct pmemstream_span_runtime sr;
	sr.type = span->type_extra & PMEMSTREAM_SPAN_TYPE_MASK;
	uint64_t extra = span->type_extra & PMEMSTREAM_SPAN_EXTRA_MASK;
	switch (sr.type) {
		case PMEMSTREAM_SPAN_FREE:
			sr.free.size = extra;
			sr.data = (uint8_t *)&span->data[0];
			sr.total_size =
				sizeof(struct pmemstream_span) + sr.free.size;
			break;
		case PMEMSTREAM_SPAN_ENTRY:
			sr.entry.size = extra;
			sr.data = (uint8_t *)&span->data[0];
			sr.total_size =
				sizeof(struct pmemstream_span) + sr.entry.size;
			break;
		case PMEMSTREAM_SPAN_REGION:
			sr.region.txid = extra;
			sr.region.size = span->data[0] -
				sizeof(struct pmemstream_span) +
				sizeof(uint64_t);
			sr.data = (uint8_t *)&span->data[1];
			sr.total_size = span->data[0];
			break;
		case PMEMSTREAM_SPAN_TX:
			sr.tx.txid = extra;
			sr.data = (uint8_t *)&span->data[0];
			sr.total_size = sizeof(struct pmemstream_span);
			break;
	}

	return sr;
}

#define PMEMSTREAM_SIGNATURE ("PMEMSTREAM")
#define PMEMSTREAM_SIGNATURE_LEN (64)
#define PMEMSTREAM_NLANES (1024)

struct pmemstream_lane {
	// persistent transaction state for each tx
	uint64_t txid;
};

struct pmemstream_header {
	char signature[PMEMSTREAM_SIGNATURE_LEN];
	uint64_t stream_size;
	uint64_t block_size;
	uint64_t txid; // todo - txid should be derived from lane data
		       // todo - unused, checksums etc
};

struct pmemstream_data {
	struct pmemstream_header header;
	struct pmemstream_lane lanes[PMEMSTREAM_NLANES];
	struct pmemstream_span spans[0];
};

struct pmemstream_tx {
	// transaction runtime state
	uint64_t txid;
	struct pmemstream *stream;
	size_t lane_id;
	struct pmemstream_lane *lane;
};

struct pmemstream {
	struct pmemstream_data *data;
	size_t stream_size;
	size_t usable_size;
	size_t block_size;

	pthread_mutex_t regions_lock;
	struct ravl *free_regions;
	// todo: regions should be composable from disjoint blocks

	pmem2_memcpy_fn memcpy;
	pmem2_memset_fn memset;
	pmem2_flush_fn flush;
	pmem2_drain_fn drain;
	pmem2_persist_fn persist;

	struct pmemstream_tx txs[PMEMSTREAM_NLANES];
};

struct pmemstream_free_region {
	struct pmemstream_region region;
	size_t size;
};

struct pmemstream_entry_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
	struct pmemstream_span *region_span;
	struct pmemstream_span_runtime region_sr;
	size_t offset;
};

struct pmemstream_region_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
};

struct pmemstream_region_context {
	// runtime state for a region, not mt-safe
	struct pmemstream_region region;
	size_t offset;
	struct pmemstream_span *last_tx_span;
};

static int
pmemstream_is_initialized(struct pmemstream *stream)
{
	if (strcmp(stream->data->header.signature, PMEMSTREAM_SIGNATURE) != 0) {
		return -1;
	}
	if (stream->data->header.block_size != stream->block_size) {
		return -1; // todo: fail with incorrect args or something
	}
	if (stream->data->header.stream_size != stream->stream_size) {
		return -1; // todo: fail with incorrect args or something
	}

	return 0;
}

static void
pmemstream_init(struct pmemstream *stream)
{
	memset(stream->data->header.signature, 0, PMEMSTREAM_SIGNATURE_LEN);
	memset(stream->data->lanes, 0, sizeof(stream->data->lanes));
	stream->data->header.stream_size = stream->stream_size;
	stream->data->header.block_size = stream->block_size;
	stream->persist(stream->data, sizeof(struct pmemstream_data));
	pmemstream_span_create_free(&stream->data->spans[0],
				    stream->usable_size -
					    sizeof(struct pmemstream_span));
	stream->persist(&stream->data->spans[0],
			sizeof(struct pmemstream_span));

	stream->memcpy(stream->data->header.signature, PMEMSTREAM_SIGNATURE,
		       PMEMSTREAM_SIGNATURE_LEN, 0);
}

static struct pmemstream_region
pmemstream_region_from_offset(size_t offset)
{
	struct pmemstream_region region;
	region.offset = offset;

	return region;
}

static struct pmemstream_span *
pmemstream_get_span_for_offset(struct pmemstream *stream, size_t offset)
{
	return (struct pmemstream_span *)((uint8_t *)stream->data->spans +
					  offset);
}

static uint64_t
pmemstream_get_offset_for_span(struct pmemstream *stream,
			       struct pmemstream_span *span)
{
	return (uint64_t)((uint64_t)span - (uint64_t)stream->data->spans);
}

static void
pmemstream_populate_free_region_list(struct pmemstream *stream)
{
	// todo: ideally we'd figure out a way to minimize the amount of time
	// required to generate the free list.
	// one idea: use external compact region metadata like in libpmemobj
	// we can also just use a persistent linked-list - but that might
	// be too costly w.r.t. performance

	size_t span_offset = 0;
	do {
		struct pmemstream_span *span =
			pmemstream_get_span_for_offset(stream, span_offset);
		struct pmemstream_span_runtime sr =
			pmemstream_span_get_runtime(span);

		switch (sr.type) {
			case PMEMSTREAM_SPAN_REGION:
				break;
			case PMEMSTREAM_SPAN_FREE: {
				struct pmemstream_free_region fregion;
				fregion.region = pmemstream_region_from_offset(
					span_offset);
				fregion.size = sr.free.size;

				ravl_emplace_copy(stream->free_regions,
						  &fregion);
			} break;
			default:
				assert(0);
		}
		span_offset += sr.total_size;
	} while (span_offset < stream->usable_size);
}

static int
region_compare(const void *lhs, const void *rhs)
{
	const struct pmemstream_free_region *l = lhs;
	const struct pmemstream_free_region *r = rhs;

	int64_t diff = (int64_t)l->size - (int64_t)r->size;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	diff = (int64_t)l->region.offset - (int64_t)r->region.offset;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	return 0;
}

static void
pmemstream_init_txs(struct pmemstream *stream)
{
	for (size_t i = 0; i < PMEMSTREAM_NLANES; ++i) {
		struct pmemstream_tx *tx = &stream->txs[i];
		tx->lane = &stream->data->lanes[i];
		tx->lane_id = i;
		tx->stream = stream;
	}
}

int
pmemstream_from_map(struct pmemstream **stream, size_t block_size,
		    struct pmem2_map *map)
{
	struct pmemstream *s = malloc(sizeof(struct pmemstream));
	s->data = pmem2_map_get_address(map);
	s->stream_size = pmem2_map_get_size(map);
	s->usable_size = ALIGN_DOWN(
		s->stream_size - sizeof(struct pmemstream_data), block_size);
	s->block_size = block_size;
	s->free_regions = ravl_new_sized(region_compare,
					 sizeof(struct pmemstream_free_region));
	pthread_mutex_init(&s->regions_lock, NULL);

	s->memcpy = pmem2_get_memcpy_fn(map);
	s->memset = pmem2_get_memset_fn(map);
	s->persist = pmem2_get_persist_fn(map);
	s->flush = pmem2_get_flush_fn(map);
	s->drain = pmem2_get_drain_fn(map);

	if (pmemstream_is_initialized(s) != 0) {
		pmemstream_init(s);
	}

	pmemstream_init_txs(s);
	pmemstream_populate_free_region_list(s);

	*stream = s;

	return 0;
}

void
pmemstream_delete(struct pmemstream **stream)
{
	struct pmemstream *s = *stream;
	pthread_mutex_destroy(&s->regions_lock);
	ravl_delete(s->free_regions);
	free(s);
	*stream = NULL;
}

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
int
pmemstream_tx_region_allocate(struct pmemstream_tx *tx,
			      struct pmemstream *stream, size_t size,
			      struct pmemstream_region *region)
{
	int ret = -1;
	pthread_mutex_lock(&stream->regions_lock);

	size = ALIGN_UP(size, stream->block_size);
	struct pmemstream_free_region fregion;
	fregion.size = size;
	fregion.region.offset = 0;

	struct ravl_node *node = ravl_find(stream->free_regions, &fregion,
					   RAVL_PREDICATE_GREATER_EQUAL);
	if (node == NULL)
		goto out;

	struct pmemstream_free_region found =
		*(struct pmemstream_free_region *)ravl_data(node);

	ravl_remove(stream->free_regions, node);

	if (found.size > size) {
		struct pmemstream_free_region new_region;
		new_region.size = found.size - size;
		new_region.region.offset = found.region.offset + size;

		struct pmemstream_span *new_span =
			pmemstream_get_span_for_offset(
				stream, new_region.region.offset);
		pmemstream_span_create_free(new_span, new_region.size);

		ravl_insert(stream->free_regions, &new_region);
	}
	struct pmemstream_span *span =
		pmemstream_get_span_for_offset(stream, found.region.offset);
	pmemstream_span_create_region(span, tx->txid, size);

	*region = found.region;

	ret = 0;

out:
	pthread_mutex_unlock(&stream->regions_lock);

	return ret;
}

int
pmemstream_tx_region_free(struct pmemstream_tx *tx,
			  struct pmemstream_region region)
{
	return 0;
}

// clearing a region is less expensive than removing it
int
pmemstream_region_clear(struct pmemstream *stream,
			struct pmemstream_region region)
{
	return 0;
}

// creates a new log transaction, this can be used to batch multiple
// stream inserts
int
pmemstream_tx_new(struct pmemstream_tx **tx, struct pmemstream *stream)
{
	// todo: lane selection...

	*tx = &stream->txs[0];

	(*tx)->txid = __atomic_fetch_add(&stream->data->header.txid, 1,
					 memory_order_acq_rel);
	// todo: this is not correct - txid that is incremented should not be in
	// the header.

	return 0;
}

// commits the written log to stable media
// synchronous transaction commit provide durable linearizability
void
pmemstream_tx_commit(struct pmemstream_tx **tx)
{
	struct pmemstream_tx *t = *tx;

	t->lane->txid = t->txid; // this marks the transaction as committed.
	t->stream->persist(&t->lane->txid, sizeof(uint64_t));

	t->txid = 0;
	t->lane->txid = 0;

	*tx = NULL;
}

// aborts a written log transaction, the consumed space will be reused
void
pmemstream_tx_abort(struct pmemstream_tx **tx)
{
	// todo
}

int
pmemstream_region_context_new(struct pmemstream_region_context **rcontext,
			      struct pmemstream *stream,
			      struct pmemstream_region region)
{
	// todo: the only reason context exists is to store the offset of the
	// place where we can append inside of the region.
	// maybe we can have this as a per-stream state in some hashtable?

	struct pmemstream_region_context *c = malloc(sizeof(*c));
	c->region = region;
	c->offset = 0;
	c->last_tx_span = NULL;

	struct pmemstream_span *region_span;
	region_span = pmemstream_get_span_for_offset(stream, region.offset);
	struct pmemstream_span_runtime region_sr =
		pmemstream_span_get_runtime(region_span);
	assert(region_sr.type == PMEMSTREAM_SPAN_REGION);

	while (c->offset < region_sr.region.size) {
		struct pmemstream_span *span =
			(struct pmemstream_span *)(region_sr.data + c->offset);
		struct pmemstream_span_runtime sr =
			pmemstream_span_get_runtime(span);
		if (sr.type == PMEMSTREAM_SPAN_TX) {
			c->last_tx_span = span;
		} else if (sr.type == PMEMSTREAM_SPAN_FREE) {
			break;
		}
		c->offset += sr.total_size;
	}

	*rcontext = c;
	return 0;
}

void
pmemstream_region_context_delete(struct pmemstream_region_context **rcontext)
{
	struct pmemstream_region_context *c = *rcontext;
	free(c);
}

// synchronously appends data buffer to the end of the transaction log space
// fails if no space is available
int
pmemstream_tx_append(struct pmemstream_tx *tx,
		     struct pmemstream_region_context *rcontext,
		     const void *buf, size_t count,
		     struct pmemstream_entry *entry)
{
	struct pmemstream_span *region_span;
	region_span = pmemstream_get_span_for_offset(tx->stream,
						     rcontext->region.offset);
	struct pmemstream_span_runtime region_sr =
		pmemstream_span_get_runtime(region_span);
	int valid_span = rcontext->last_tx_span != NULL;
	struct pmemstream_span_runtime tx_sr;
	if (valid_span) {
		tx_sr = pmemstream_span_get_runtime(rcontext->last_tx_span);
		assert(tx_sr.type == PMEMSTREAM_SPAN_TX);
		valid_span = tx_sr.tx.txid == tx->txid;
	}
	if (!valid_span) {
		rcontext->last_tx_span =
			(struct pmemstream_span *)(region_sr.data +
						   rcontext->offset);
		pmemstream_span_create_tx(rcontext->last_tx_span, tx->txid);
		tx_sr = pmemstream_span_get_runtime(rcontext->last_tx_span);
		rcontext->offset += tx_sr.total_size;
	}

	struct pmemstream_span *entry_span =
		(struct pmemstream_span *)(region_sr.data + rcontext->offset);
	if (entry) {
		entry->offset =
			pmemstream_get_offset_for_span(tx->stream, entry_span);
	}

	pmemstream_span_create_entry(entry_span, count);
	struct pmemstream_span_runtime entry_rt =
		pmemstream_span_get_runtime(entry_span);
	// TODO: this check is too late, we haven't check if there's space for
	// span metadata
	if (region_sr.region.size < rcontext->offset + entry_rt.total_size) {
		return -1;
	}

	rcontext->offset += entry_rt.total_size;

	tx->stream->memcpy(entry_rt.data, buf, count, PMEM2_F_MEM_NODRAIN);

	return 0;
}

// returns pointer to the data of the entry
void *
pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct pmemstream_span *entry_span;
	entry_span = pmemstream_get_span_for_offset(stream, entry.offset);
	struct pmemstream_span_runtime entry_sr =
		pmemstream_span_get_runtime(entry_span);
	assert(entry_sr.type == PMEMSTREAM_SPAN_ENTRY);

	return entry_sr.data;
}

// returns the size of the entry
size_t
pmemstream_entry_length(struct pmemstream *stream,
			struct pmemstream_entry entry)
{
	struct pmemstream_span *entry_span;
	entry_span = pmemstream_get_span_for_offset(stream, entry.offset);
	struct pmemstream_span_runtime entry_sr =
		pmemstream_span_get_runtime(entry_span);
	assert(entry_sr.type == PMEMSTREAM_SPAN_ENTRY);

	return entry_sr.entry.size;
}

// an active pmemstream region or entry prevents the truncate function from
// removing its memory location.
// truncation can only affect regions.

int
pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator,
			       struct pmemstream *stream)
{
	struct pmemstream_region_iterator *iter = malloc(sizeof(*iter));
	iter->stream = stream;
	iter->region.offset = 0;

	*iterator = iter;

	return 0;
}

int
pmemstream_region_iterator_next(struct pmemstream_region_iterator *iter,
				struct pmemstream_region *region)
{
	struct pmemstream_span *region_span;
	struct pmemstream_span_runtime region_sr;

	while (iter->region.offset < iter->stream->usable_size) {
		region_span = pmemstream_get_span_for_offset(
			iter->stream, iter->region.offset);
		region_sr = pmemstream_span_get_runtime(region_span);

		if (region_sr.type == PMEMSTREAM_SPAN_REGION) {
			*region = iter->region;
			iter->region.offset += region_sr.total_size;
			return 0;
		}
		assert(region_sr.type == PMEMSTREAM_SPAN_FREE);
		iter->region.offset += region_sr.total_size;
	}

	return -1;
}

void
pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator)
{
	struct pmemstream_region_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}

int
pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator,
			      struct pmemstream *stream,
			      struct pmemstream_region region)
{
	struct pmemstream_entry_iterator *iter = malloc(sizeof(*iter));
	iter->offset = 0;
	iter->region = region;
	iter->stream = stream;
	iter->region_span =
		pmemstream_get_span_for_offset(stream, region.offset);
	iter->region_sr = pmemstream_span_get_runtime(iter->region_span);

	*iterator = iter;

	return 0;
}

int
pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter,
			       struct pmemstream_region *region,
			       struct pmemstream_entry *entry)
{
	for (;;) {
		if (iter->offset >= iter->region_sr.region.size) {
			return -1;
		}
		struct pmemstream_span *entry_span =
			(struct pmemstream_span *)(iter->region_sr.data +
						   iter->offset);

		struct pmemstream_span_runtime rt =
			pmemstream_span_get_runtime(entry_span);
		iter->offset += rt.total_size;

		switch (rt.type) {
			case PMEMSTREAM_SPAN_FREE:
				return -1;
				break;
			case PMEMSTREAM_SPAN_TX:
				continue;
				break;
			case PMEMSTREAM_SPAN_REGION:
				return -1;
				break;
			case PMEMSTREAM_SPAN_ENTRY: {

				if (entry) {
					entry->offset =
						pmemstream_get_offset_for_span(
							iter->stream,
							entry_span);
				}
				if (region) {
					*region = iter->region;
				}
				return 0;
			} break;
		}
	}

	return -1;
}

void
pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}
