// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "stream_span_helpers.hpp"

#include "../src/libpmemstream_internal.h"
#include "../src/span.h"
#include "stream_helpers.hpp"

std::vector<span_runtime> span_runtimes_from_stream(const pmem::stream &stream, size_t offset, size_t end_offset)
{
	std::vector<span_runtime> spans;
	auto span_bytes_ptr = reinterpret_cast<const char *>(stream.c_ptr()->data.base);
	end_offset = std::min(end_offset, stream.c_ptr()->usable_size);

	while (offset < end_offset) {
		auto span_base_ptr = reinterpret_cast<const span_base *>(span_bytes_ptr + offset);
		auto span_type = span_get_type(span_base_ptr);
		span_runtime sr{offset, span_base_ptr, {}};
		if (span_type == SPAN_REGION) {
			sr.sub_spans = span_runtimes_from_stream(stream, offset + sizeof(span_region),
								 offset + span_get_total_size(span_base_ptr));
			spans.emplace_back(std::move(sr));
		} else if (span_type == SPAN_EMPTY && spans.size() && span_get_type(spans.back().ptr) == SPAN_EMPTY) {
			/* Skip adding multiple, adjacent empty spans. */
		} else {
			spans.emplace_back(std::move(sr));
		}

		offset += span_get_total_size(span_base_ptr);
	}

	return spans;
}

std::string span_to_str(const struct span_base *base)
{
	std::map<uint64_t, const std::string> span_type_names = {{SPAN_ENTRY, std::string("entry")},
								 {SPAN_REGION, std::string("region")},
								 {SPAN_EMPTY, std::string("empty")}};

	return "type: " + span_type_names[span_get_type(base)] + ", data size: " + std::to_string(span_get_size(base));
}

std::ostream &operator<<(std::ostream &os, const struct span_base *base)
{
	return os;
	// return (os << span_to_str(base));
}

std::ostream &operator<<(std::ostream &os, const std::vector<span_runtime> &spans)
{
	return os;
	// return show_spans(os, spans);
}
