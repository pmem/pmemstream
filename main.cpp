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

int main() {
	cout << "main()" << endl;
	int size = 64*100;
	int blk_size = 64;
	const mode_t FILE_MODE = 0644;
	const char* path = "mapped_file.dat";

	struct pmemstream *s = NULL;
	struct pmem2_map *map = map_open(path, size);

	pmemstream_from_map(&s, blk_size, map);
	// UT_ASSERTne(s, NULL);

	// cout << map << endl;
	// cout <<  << endl;
	// printf("%s", map+10);

	pmemstream_delete(&s);
	pmem2_map_delete(&map);

	return 0;
}
