---
draft: false
layout: "library"
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["libpmemstream.3.html"]
title: libpmemstream
section: 3
secondary_title: pmemstream
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (libpmemstream.3 -- man page for libpmemstream API)

[NAME](#name)\
[SYNOPSIS](#synopsis)\
[DESCRIPTION](#description)\
[SEE ALSO](#see-also)


# NAME #

**libpmemstream** - a logging data structure optimized for persistent memory.

# SYNOPSIS #

```c
#include <libpmemstream.h>

struct pmemstream;
struct pmemstream_entry_iterator;
struct pmemstream_region_iterator;
struct pmemstream_region_runtime;
struct pmemstream_region {
	uint64_t offset;
};

struct pmemstream_entry {
	uint64_t offset;
};

struct pmemstream_async_wait_data;
struct pmemstream_async_wait_output {
	int error_code;
};

FUTURE(pmemstream_async_wait_fut,
	struct pmemstream_async_wait_data, struct pmemstream_async_wait_output);

int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map);
void pmemstream_delete(struct pmemstream **stream);

int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region);
int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region);

size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region);
size_t pmemstream_region_usable_size(struct pmemstream *stream, struct pmemstream_region region);

int pmemstream_region_runtime_initialize(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime **runtime);

int pmemstream_reserve(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, size_t size,
		       struct pmemstream_entry *reserved_entry, void **data);
int pmemstream_publish(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry entry, size_t size);
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region region,
		      struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
		      struct pmemstream_entry *new_entry);

int pmemstream_async_publish(struct pmemstream *stream, struct pmemstream_region region,
			     struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry entry,
			     size_t size);
int pmemstream_async_append(struct pmemstream *stream, struct vdm *vdm, struct pmemstream_region region,
			    struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
			    struct pmemstream_entry *new_entry);

uint64_t pmemstream_committed_timestamp(struct pmemstream *stream);
uint64_t pmemstream_persisted_timestamp(struct pmemstream *stream);

struct pmemstream_async_wait_fut pmemstream_async_wait_committed(struct pmemstream *stream, uint64_t timestamp);
struct pmemstream_async_wait_fut pmemstream_async_wait_persisted(struct pmemstream *stream, uint64_t timestamp);

const void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry);
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry);
uint64_t pmemstream_entry_timestamp(struct pmemstream *stream, struct pmemstream_entry entry);

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region);

int pmemstream_entry_iterator_is_valid(struct pmemstream_entry_iterator *iterator);
void pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator);
void pmemstream_entry_iterator_seek_first(struct pmemstream_entry_iterator *iterator);
struct pmemstream_entry pmemstream_entry_iterator_get(struct pmemstream_entry_iterator *iterator);
void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator);

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream);
int pmemstream_region_iterator_is_valid(struct pmemstream_region_iterator *iterator);
void pmemstream_region_iterator_seek_first(struct pmemstream_region_iterator *iterator);
void pmemstream_region_iterator_next(struct pmemstream_region_iterator *iterator);
struct pmemstream_region pmemstream_region_iterator_get(struct pmemstream_region_iterator *iterator);
void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator);
```

# DESCRIPTION #

<TBD>
Functions' descriptions

# SEE ALSO #

**libpmem2**(7), **miniasync**(7), **libpmemstream**(7), and **<https://pmem.io/pmemstream>**
