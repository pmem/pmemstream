// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * iterate_validation.cpp -- verifies if randomly generated (inconsistent) spans are not
 *                           treated by pmemstream iterators as valid entries.
 */

#include <cstring>
#include <string>
#include <vector>

#include <rapidcheck.h>

#include "common/util.h"
#include "libpmemstream_internal.h"
#include "rapidcheck_helpers.hpp"
#include "span.h"
#include "stream_helpers.hpp"
#include "stream_span_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check(
			"verify if stream does not treat inconsistent spans as valid entries",
			[&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data) {
				RC_PRE(data.size() > 0);
				stream.helpers.append(stream.helpers.get_first_region(), data);

				auto span_view = span_runtimes_from_stream(stream.sut);
				UT_ASSERTeq(span_get_type(span_view[0].ptr), SPAN_REGION);
				auto &region = span_view[0];
				auto &entries = region.sub_spans;
				auto expected_num_spans = data.size() + 1 /* one empty span. */;
				UT_ASSERTeq(entries.size(), expected_num_spans);
				for (size_t i = 0; i < data.size(); i++) {
					auto data_ptr =
						reinterpret_cast<const char *>(entries[i].ptr) + sizeof(span_entry);
					UT_ASSERT(std::string(data_ptr, span_get_size(entries[i].ptr)) == data[i]);
				}
			});
	});
}
