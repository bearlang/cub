#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// TODO: make this actually do this correctly
uint8_t *bear_allocate(uint64_t count, uint32_t storecount, uint8_t *ptr) {
	return malloc(count);
}

bool bear_streq(uint8_t *a, uint8_t *b) {
	if (a == b) {
		return true;
	}
	uint64_t la = *(uint64_t*) a, lb = *(uint64_t*) b;
	la &= ~0x8000000000000000;
	lb &= ~0x8000000000000000;
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

void bear_print_number(uint64_t value/*, uint8_t *str*/) {
	printf("%lu\n", value);
/*	uint64_t slen = *(uint64_t*) bstr;
	slen &= ~0x8000000000000000;
	char temp[slen + 1];
	memcpy(temp, (uint8_t*) (1 + (uint64_t*) bstr), slen);
	temp[slen] = 0;
	printf("%lu %s\n", value, temp);*/
}
