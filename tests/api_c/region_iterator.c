#include "unittest.h"


void test_region_iterator(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_region_iterator *riter;

    pmemstream_region_iterator_new(&riter, stream);
    UT_ASSERTne(riter, NULL);

    pmemstream_region_iterator_next(riter, &region);
    UT_ASSERTne(riter, NULL);

	pmemstream_region_iterator_delete(&riter);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();

    struct pmem2_map *map = map_open(argv[1], 10240, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, 4096, map);

    struct pmemstream_region *region;
	
	pmemstream_region_allocate(stream, 1024, region);

    test_region_iterator(NULL, NULL);
    test_region_iterator(stream, NULL);
    test_region_iterator(NULL, region);
    test_region_iterator(stream, region);

    pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
