// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_STREAM_SPAN_HELPERS_HPP
#define LIBPMEMSTREAM_STREAM_SPAN_HELPERS_HPP

#include <iostream>
#include <vector>

struct span_base;
namespace pmem
{
struct stream;
}

struct span_runtime {
	size_t offset;
	const span_base *ptr;
	std::vector<span_runtime> sub_spans;
};

std::vector<span_runtime> span_runtimes_from_stream(const pmem::stream &stream, size_t offset = 0,
						    size_t end_offset = UINT64_MAX);
std::string span_to_str(const struct span_base *base);
std::ostream &operator<<(std::ostream &os, const struct span_base *base);
std::ostream &operator<<(std::ostream &os, const std::vector<span_runtime> &spans);

#endif /* LIBPMEMSTREAM_STREAM_SPAN_HELPERS_HPP */
