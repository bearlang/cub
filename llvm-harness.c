#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// TODO: make this actually do this correctly
uint8_t *bear_allocate(uint64_t count) {
	return malloc(count);
}

bool bear_streq(uint8_t *a, uint8_t *b) {
	if (a == b) {
		return true;
	}
	uint64_t la = *(uint64_t*) a, lb = *(uint64_t*) b;
	if (la != lb) {
		return false;
	}
	uint8_t *ra = (uint8_t*) (1 + (uint64_t*) a), *rb = (uint8_t*) (1 + (uint64_t*) b);
	for (uint64_t i = 0; i < la; i++) {
		if (ra[i] != rb[i]) {
			return false;
		}
	}
	return true;
}
