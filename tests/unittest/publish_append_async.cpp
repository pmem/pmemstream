// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * publish_append_async.cpp -- pmemstream_async_publish and pmemstream_async_append functional test.
 */

#include <cstring>
#include <libminiasync.h>
#include <libminiasync/vdm.h>
#include <vector>

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		UT_FATAL("usage: %s file-path", argv[0]);
	}

	struct test_config_type test_config;
	test_config.filename = argv[1];

	return run_test(test_config, [&] {
		return_check ret;

		/* XXX: lower the TCs number for memcheck */
		// std::string rapidcheck_config = "noshrink=1 max_success=5 max_size=5";
		// env_setter setter("RC_PARAMS", rapidcheck_config, false);

		/* XXX: add stateful test(s) with mixed commands (even without model; just to randomize commands) */
		ret += rc::check("verify if mixing regular appends with async appends works fine",
				 [&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, const bool async_first) {
					 auto region = stream.helpers.get_first_region();

					 if (async_first) {
						 stream.helpers.async_append(region, data);
						 stream.helpers.append(region, extra_data);
					 } else {
						 stream.helpers.append(region, data);
						 stream.helpers.async_append(region, extra_data);
					 }

					 stream.helpers.verify(region, data, extra_data);

					 UT_ASSERTeq(stream.sut.region_free(region), 0);
				 });

		ret += rc::check("verify if mixing regular reserve+publish with async appends works fine",
				 [&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data, const bool async_first) {
					 auto region = stream.helpers.get_first_region();

					 if (async_first) {
						 stream.helpers.async_append(region, data);
						 stream.helpers.reserve_and_publish(region, extra_data);
					 } else {
						 stream.helpers.reserve_and_publish(region, data);
						 stream.helpers.async_append(region, extra_data);
					 }

					 stream.helpers.verify(region, data, extra_data);

					 UT_ASSERTeq(stream.sut.region_free(region), 0);
				 });
	});
}
