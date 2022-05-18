// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "region.h"

#include <vector>

#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
static constexpr size_t concurrency = 100;
static constexpr size_t num_repeats = 1000;

auto make_region_runtimes_map = make_instance_ctor(region_runtimes_map_new, region_runtimes_map_destroy);
} // namespace

int main(int argc, char *argv[])
{
	return run_test([&] {
		{
			auto region_runtimes_map = make_region_runtimes_map(nullptr); // XXX: mock pmemstream_runtime

			struct pmemstream_region_runtime *region_runtime;
			struct pmemstream_region region = {8};
			int ret = region_runtimes_map_get_or_create(region_runtimes_map.get(), region, &region_runtime);
			UT_ASSERTeq(ret, 0);

			UT_ASSERTeq(region_runtime_get_state_acquire(region_runtime), REGION_RUNTIME_STATE_READ_READY);
			syncthreads_barrier syncthreads(concurrency);
			parallel_exec(concurrency, [&](size_t id) {
				if (id % 2 == 0) {
					syncthreads();

					region_runtime_initialize_for_write_locked(region_runtime, region.offset);

					UT_ASSERTeq(region_runtime_get_state_acquire(region_runtime),
						    REGION_RUNTIME_STATE_WRITE_READY);
				} else {
					syncthreads();

					for (size_t i = 0; i < num_repeats; i++) {
						auto initialized = region_runtime_get_state_acquire(region_runtime) ==
							REGION_RUNTIME_STATE_WRITE_READY;
						if (initialized) {
							auto append_offset = region_runtime_get_append_offset_acquire(
								region_runtime);
							UT_ASSERTeq(append_offset, region.offset);
						}
					}
				}
			});

			UT_ASSERTeq(region_runtime_get_state_acquire(region_runtime), REGION_RUNTIME_STATE_WRITE_READY);

			region_runtimes_map_remove(region_runtimes_map.get(), region);
		}
	});
}
