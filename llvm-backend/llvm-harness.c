#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <math.h>

// #define TRACE_GC

enum reachable {
	ALPHA=0,
	BETA=1
};

struct metafield {
	uint32_t offset;
	uint8_t is_string;
	struct metafield *next;
};

struct metastruct {
	uint32_t length;
	uint32_t struct_id;
	struct metafield *mf;
};

struct gcinfo {
	struct metastruct *meta;
	enum reachable reachable;
	struct gcinfo *next;
};

#ifdef TRACE_GC
#define INDENT(indent) for (int i=0; i<indent; i++) { putchar('\t'); }
#else
#define INDENT(indent)
#endif

static inline void enumerate_string(int indent, uint8_t *data) {
	INDENT(indent)
	if ((*(uint64_t*)data) & 0x8000000000000000) {
		// static
#ifdef TRACE_GC
		printf("static string\n");
#endif
	} else {
		// TODO: implement
#ifdef TRACE_GC
		printf("heap string\n");
#endif
	}
}

enum reachable unreachable = ALPHA;

static inline void enumerate_object(int indent, uint8_t *data) {
	INDENT(indent);
	if (data == NULL) {
#ifdef TRACE_GC
		printf("NULL\n");
#endif
		return;
	}
	fflush(stdout);
	struct gcinfo *gcinfo = &((struct gcinfo *) data)[-1];
	struct metastruct *meta = gcinfo->meta;
	if (gcinfo->reachable != unreachable) {
#ifdef TRACE_GC
		printf("heap object at %lu of type %u already marked.\n", (uint64_t) data, meta->struct_id);
#endif
		return;
	}
	gcinfo->reachable = !unreachable;
#ifdef TRACE_GC
	printf("heap object at %lu of type %u", (uint64_t) data, meta->struct_id);
#endif
	struct metafield *cur = meta->mf;
	while (cur != NULL) {
		INDENT(indent);
#ifdef TRACE_GC
		printf("    at +%u:\n", cur->offset);
#endif
		uint8_t *value = *(uint8_t**) (data + cur->offset);
		if (cur->is_string) {
			enumerate_string(indent + 1, value);
		} else {
			enumerate_object(indent + 1, value);
		}
		cur = cur->next;
	}
}

static inline void enumerate_objects_raw(int indent, void *data) {
	if (((uintptr_t) data) & 1) { // a string
		uint8_t *ptr = (uint8_t*) (((uintptr_t) data) & ~1);
		enumerate_string(indent, ptr);
	} else { // an object
		enumerate_object(indent, (uint8_t*) data);
	}
}

struct gcinfo *chain_head = NULL;

static void garbage_collect() {
	struct gcinfo *cur = chain_head;
	struct gcinfo *last = NULL;
	while (cur != NULL) {
		if (cur->reachable == unreachable) {
#ifdef TRACE_GC
			printf("\tDeallocating: %lu\n", (uint64_t) (cur + 1));
#endif
			if (last == NULL) {
				chain_head = cur->next;
				free(cur);
				cur = chain_head;
			} else {
				last->next = cur->next;
				free(cur);
				cur = last->next;
			}
		} else {
#ifdef TRACE_GC
			printf("\tPreserving: %lu\n", (uint64_t) (cur + 1));
#endif
			last = cur;
			cur = cur->next;
		}
	}
	// everything remaining is marked as reachable
	unreachable = !unreachable;
	// now everything remaining is marked as unreachable and we're ready for another round
}

// TODO: make this actually do this correctly
uint8_t *bear_new(struct metastruct *mts, uint32_t storecount, void **ptr) { // TODO: check for overflow
#ifdef TRACE_GC
	printf("\nEnumerating object map...\n");
#endif
	for (uint32_t i = 0; i < storecount; i++) {
#ifdef TRACE_GC
		printf("Root %u:\n", i);
#endif
		enumerate_objects_raw(1, ptr[i]);
	}
	garbage_collect();
#ifdef TRACE_GC
	printf("ALLOCATING %lu (%lu)\n", mts->struct_id, (uint64_t) mts);
#endif
	struct gcinfo *out = malloc(mts->length + sizeof(struct gcinfo));
	out->meta = mts;
	out->next = chain_head;
	out->reachable = unreachable;
	chain_head = out;
	uint8_t *real_out = (uint8_t*) (out + 1);
	if (((uintptr_t) real_out) & 1) {
		printf("Bad alignment.");
		abort();
	}
#ifdef TRACE_GC
	printf("ADDR: %lu\n", (uint64_t) real_out);
#endif
	return real_out;
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

void bear_print(uint8_t *head) {
	uint64_t length = (*(uint64_t*) head) & ~0x8000000000000000;
	fwrite(head + 8, sizeof(*head), length, stdout);
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

uint8_t bear_log2(uint8_t value) {
	return log2f(value);
}

uint8_t bear_log10(uint8_t value) {
	return log10f(value);
}
