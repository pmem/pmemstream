#include "unittest.h"


void test_entry_iterator(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_entry_iterator *eiter;
    struct pmemstream_entry entry;

    pmemstream_entry_iterator_new(&eiter, stream, *region);
    UT_ASSERTne(eiter, NULL);

    pmemstream_entry_iterator_next(eiter, NULL, &entry);
    UT_ASSERTne(eiter, NULL);

	pmemstream_entry_iterator_delete(&eiter);
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

    test_entry_iterator(NULL, NULL);
    test_entry_iterator(stream, NULL);
    test_entry_iterator(NULL, region);
    test_entry_iterator(stream, region);

    pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
