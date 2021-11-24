#include <stdint.h>
#include <stddef.h>

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define ALIGN_DOWN(size, align) ((size) & ~((align)-1))

static inline unsigned char
util_popcount64(uint64_t value)
{
	return (unsigned char)__builtin_popcountll(value);
}

static inline uint64_t
pmemstream_popcount(const uint64_t *data, size_t size)
{
	uint64_t count = 0;
	size_t i = 0;
	const uint8_t *d = (const uint8_t*)&data[0];

	for (; i < ALIGN_DOWN(size, sizeof(uint64_t)); i += sizeof(uint64_t)) {
		count += util_popcount64(*(const uint64_t*)(d + i));
	}
	for (; i < size; i++) {
		count += util_popcount64(d[i]);
	}

	return count;
}
