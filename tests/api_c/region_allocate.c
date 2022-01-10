#include "unittest.h"


void test_pmemstream_region_allocation(char *path, size_t size)
{
	struct pmem2_map *map = map_open(path, 10240, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, 4096, map);

	struct pmemstream_region region;
	
	pmemstream_region_allocate(stream, size, &region);
	UT_ASSERTne(&region, NULL);

	size_t s;
	s = pmemstream_region_size(stream, region);

	struct pmemstream_region_context *ctx = NULL;
	pmemstream_get_region_context(stream, region, &ctx);
	UT_ASSERTne(ctx, NULL);

	pmemstream_region_free(stream, region);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();
	char *path = argv[1];

	test_stream_region_allocation(path, NULL);
	test_stream_region_allocation(path, 4096);

	return 0;
}
