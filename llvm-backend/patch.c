#include "patch.h"

struct patchent *fent = NULL;
struct patchvar *fvar = NULL;

void pt_reset() {
	{
		struct patchent *cur = fent;
		while (cur != NULL) {
			struct patchent *next = cur->next;
			if (!cur->is_ref) {
				free(cur->data);
			}
			free(cur);
			cur = next;
		}
	}
	{
		struct patchvar *cur = fvar;
		while (cur != NULL) {
			struct patchvar *next = cur->next;
			free(cur->value);
			free(cur);
			cur = next;
		}
	}
	fent = NULL;
	fvar = NULL;
}

void pt_finalize(FILE *out) {
	struct patchent *cur = fent;
	struct patchent *last = NULL;

	while (cur != NULL) { // reverse the linked list
		struct patchent *next = cur->next;
		cur->next = last;
		last = cur;
		cur = next;
	}
	fent = last;

	cur = fent;
	while (cur != NULL) {
		if (cur->is_ref) {
			if (cur->ref->value == NULL) {
				fputs("variable not completed\n", stderr);
				exit(1);
			}
			fputs(cur->ref->value, out);
		} else {
			fputs(cur->data, out);
		}
		cur = cur->next;
	}

	pt_reset();
}

struct patchvar *pt_def() {
	struct patchvar *out = malloc(sizeof(struct patchvar));
	out->next = fvar;
	fvar = out;
	out->value = NULL;
	return out;
}

void pt_put(struct patchvar *var, char *value) {
	if (value == NULL) {
		fputs("value is NULL\n", stderr);
		exit(1);
	}
	if (var->value != NULL) {
		free(var->value);
	}
	var->value = value;
}

const char *pt_fetch(struct patchvar *var) {
	if (var->value == NULL) {
		fputs("fetch on NULL var\n", stderr);
		exit(1);
	}
	return var->value;
}

struct patchent *pt_new_ent(bool is_ref) {
	struct patchent *out = malloc(sizeof(struct patchent));
	out->next = fent;
	fent = out;
	out->is_ref = is_ref;
	return out;
}

void pt_use(struct patchvar *var) {
	pt_new_ent(true)->ref = var;
}

void pt_write(char *text) {
	pt_new_ent(false)->data = text;
}
