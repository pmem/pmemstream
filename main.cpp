#include <libpmemstream.h>
#include <iostream>

#include <errno.h>
#include <fcntl.h>
#include <libpmem2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

static inline struct pmem2_map *map_open(const char *file, size_t size)
{
	const mode_t FILE_MODE = 0644;
	struct pmem2_source *source;
	struct pmem2_config *config;
	struct pmem2_map *map = NULL;

	int fd = open(file, O_CREAT | O_RDWR | O_TRUNC, FILE_MODE);
	if (fd < 0) {
		// UT_FATAL("File creation error (errno: %d)!", errno);
		return NULL;
	}

	if (size > 0) {
		if (ftruncate(fd, (off_t)size) != 0)
			goto err_fd;
	}

	if (pmem2_source_from_fd(&source, fd) != 0)
		goto err_fd;

	if (pmem2_config_new(&config) != 0)
		goto err_config;

	pmem2_config_set_required_store_granularity(config, PMEM2_GRANULARITY_PAGE);

	if (pmem2_map_new(&map, config, source) != 0)
		goto err_map;

err_map:
	pmem2_config_delete(&config);
err_config:
	pmem2_source_delete(&source);
err_fd:
	close(fd);

	// UT_ASSERTne(map, NULL);
	return map;
}

struct data_entry {
	uint64_t data;
};

int main() {
	cout << "main()" << endl;
	int size = 64*100;
	int blk_size = 64;
	const mode_t FILE_MODE = 0644;
	const char* path = "mapped_file.dat";

	struct pmemstream *s = NULL;
	struct pmem2_map *map = map_open(path, size);

	auto ret = pmemstream_from_map(&s, blk_size, map);
	if (ret == -1) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return ret;
	}

	struct pmemstream_region new_region;
	ret = pmemstream_region_allocate(s, 1024, &new_region);
	cout << "Region allocation status: " << ret << endl;

	struct pmemstream_region_iterator *riter;
	ret = pmemstream_region_iterator_new(&riter, s);
	if (ret == -1) {
		fprintf(stderr, "pmemstream_region_iterator_new failed\n");
		return ret;
	}
	// pmemstream_region_iterator_delete(&riter);


	struct pmemstream_region region;
	ret = pmemstream_region_iterator_next(riter, &region);
	cout << "Region iterator status: " << ret << endl;
	pmemstream_region_iterator_delete(&riter);

	struct pmemstream_entry entry;
	struct pmemstream_entry new_entry;
	long long e = 0xBEEF;
	cout << "Region offset: " << new_region.offset << endl;
	ret = pmemstream_append(s, &new_region, &entry, (void*)&e, sizeof(e), &new_entry);
	cout << "Append status: " << ret << endl;
	// ret = pmemstream_region_allocate(s, 0xCA, &new_region);
	// cout << "Region allocation status: " << ret << endl;

	// pmemstream_entry_iterator_delete(&eiter);

	pmemstream_delete(&s);
	pmem2_map_delete(&map);

	return 0;
}
