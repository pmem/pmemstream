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

/* XXX: create similar test for region_runtime_initialize_clear_locked */
int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	auto path = std::string(argv[1]);

	return run_test([&] {
		{
			auto region_runtimes_map = make_region_runtimes_map();

			struct pmemstream_region_runtime *region_runtime;
			struct pmemstream_region region = {8};
			struct span_runtime region_srt;
			region_srt.total_size = 0;

			// XXX: fille region_srt properly

			int ret = region_runtimes_map_get_or_create(region_runtimes_map.get(), region, region_srt,
								    &region_runtime);
			UT_ASSERTeq(ret, 0);

			UT_ASSERTeq(region_runtime_get_state_acquire(region_runtime),
				    REGION_RUNTIME_STATE_UNINITIALIZED);
			parallel_xexec(concurrency, [&](size_t id, std::function<void(void)> syncthreads) {
				if (id % 2 == 0) {
					struct pmemstream_entry entry {
						region.offset
					};

					syncthreads();

					region_runtime_initialize_dirty_locked(region_runtime, entry);

					UT_ASSERTeq(region_runtime_get_state_acquire(region_runtime),
						    REGION_RUNTIME_STATE_DIRTY);
				} else {
					syncthreads();

					for (size_t i = 0; i < num_repeats; i++) {
						auto initialized = region_runtime_get_state_acquire(region_runtime) !=
							REGION_RUNTIME_STATE_UNINITIALIZED;
						if (initialized) {
							UT_ASSERTeq(region_runtime_get_state_acquire(region_runtime),
								    REGION_RUNTIME_STATE_DIRTY);
						}
					}
				}
			});

			UT_ASSERTeq(region_runtime_get_state_acquire(region_runtime), REGION_RUNTIME_STATE_DIRTY);

			region_runtimes_map_remove(region_runtimes_map.get(), region);
		}
	});
}
