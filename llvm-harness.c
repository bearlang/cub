#include <stdlib.h>
#include <stdint.h>

// TODO: make this actually do this correctly
uint8_t *allocate(uint64_t count) {
	return malloc(count);
}
