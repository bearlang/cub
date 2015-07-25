#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "backend.h"

struct patchent {
	bool is_ref;
	union {
		char *data;
		struct patchvar *ref;
	};
	struct patchent *next;
};

struct patchvar {
	char *value;
	struct patchvar *next;
};

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

#define pf(...) { char *cptr; if (asprintf(&cptr, __VA_ARGS__) == -1) { perror("asprintf"); exit(1); } pt_write(cptr); }
#define pv(var) { pt_use(var); }
#define vf(var, ...) { char *cptr; if (asprintf(&cptr, __VA_ARGS__) == -1) { perror("asprintf"); exit(1); } pt_put(var, cptr); }
#define vfr(var, x) { pt_put(var, x); }

static void wt(type *t) {
	if (t == NULL) {
		abort();
	}
	switch (t->type) {
	case T_ARRAY:
		fprintf(stderr, "array type not implemented\n");
		exit(1);
	case T_BLOCKREF:
		pf("void (");
		argument *arg = t->blocktype;
		if (arg != NULL) {
			wt(arg->argument_type);
			arg = arg->next;
			for (; arg != NULL; arg = arg->next) {
				pf(", ");
				wt(arg->argument_type);
			}
		}
		pf(")*");
		break;
	case T_BOOL:
		pf("i1");
		break;
	case T_F32:
		pf("float");
		break;
	case T_F64:
		pf("double");
		break;
	case T_F128:
		pf("fp128");
		break;
	case T_OBJECT:
		pf("%%struct.%zu*", t->struct_index);
		break;
	case T_REF:
		abort();
	case T_U8:
	case T_S8:
		pf("i8"); // in LLVM, signedness is distinguished on the instructions, not the types
		break;
	case T_U16:
	case T_S16:
		pf("i16");
		break;
	case T_U32:
	case T_S32:
		pf("i32");
		break;
	case T_U64:
	case T_S64:
		pf("i64");
		break;
	case T_STRING:
		pf("%%struct.string*");
		break;
	case T_VOID:
		pf("void");
		break;
	default:
		abort();
	}
}

void backend_write(code_system *system, FILE *out) {
	pt_reset();

	pf("target datalayout = \"e-m:e-i64:64-f80:128-n8:16:32:64-S128\"\n"
		 "target triple = \"x86_64-unknown-linux-gnu\"\n\n\n");

	if (system->struct_count) {
		for (size_t i = 0; i < system->struct_count; i++) {
			code_struct *str = &system->structs[i];

			pf("%%struct.%zu = type { ", i);
			for (size_t j = 0; j < str->field_count; j++) {
				if (j != 0) {
					pf(", ");
				}
				wt(str->fields[j].field_type);
			}
			pf(" };\n");
		}
		pf("\n\n");
	}

	pf("declare i8* @allocate(i64) nounwind\n");
	pf("declare void @exit(i32) nounwind noreturn\n\n");

	pf("define fastcc i32 @main() {\n"
					"  tail call fastcc void @block_0()\n"
					"  ret i32 0\n"
					"}\n\n");

	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = &system->blocks[i];

		pf("define internal fastcc void @block_%zu(", i);

		size_t offset = block->parameter_count;
		if (offset) {
			wt(block->parameters[0].field_type);

			for (size_t j = 1; j < offset; j++) {
				pf(", ");
				wt(block->parameters[j].field_type);
			}
		}

		pf(") #0 {\n"); // TODO: include noreturn

#define TYPEOF(x) ((x) < offset ? block->parameters[x].field_type : block->instructions[x - offset].type)

		struct patchvar *ref[block->instruction_count + offset];

#define RP(n) pt_fetch(ref[ins->parameters[(n)]])
#define PR(n) pv(ref[ins->parameters[(n)]]);
#define TP(n) (TYPEOF(ins->parameters[(n)]))

		for (size_t k = 0; k < offset + block->instruction_count; k++) {
			ref[k] = pt_def();
			vf(ref[k], k < offset ? "%%%zu" : "%%n%zu", k);
		}

		for (size_t j = 0; j < block->instruction_count; j++) {
			size_t k = j + offset;
			code_instruction *ins = &block->instructions[j];

			switch (ins->operation.type) {
			case O_BITWISE_NOT:
				pf("  %%n%zu = xor ", k);
				wt(ins->type);
				pf(" %s, -1", RP(0));
				break;
			case O_BLOCKREF:
				vf(ref[k], "@block_%zu", ins->block_index);
				break;
			case O_CAST: {
				type *typ = ins->type;
				const char *name = NULL;
				int np = 0;
				switch (ins->operation.cast_type) {
				case O_UPCAST:
				case O_DOWNCAST: // TODO: check downcasts
					name = "bitcast";
					typ = &(type) {.type=T_OBJECT, .struct_index = ins->parameters[np++]};
					break;
				case O_FLOAT_EXTEND:
					name = "fpext";
					break;
				case O_FLOAT_TRUNCATE:
					name = "fptrunc";
					break;
				case O_FLOAT_TO_SIGNED:
					name = "fptosi";
					break;
				case O_FLOAT_TO_UNSIGNED:
					name = "fptoui";
					break;
				case O_SIGNED_TO_FLOAT:
					name = "sitofp";
					break;
				case O_UNSIGNED_TO_FLOAT:
					name = "uitofp";
					break;
				case O_SIGN_EXTEND:
					name = "sext";
					break;
				case O_ZERO_EXTEND:
					name = "zext";
					break;
				case O_TRUNCATE:
					name = "trunc";
					break;
				case O_REINTERPRET:
					name = "bitcast";
					break;
				}
				pf("  %%n%zu = %s ", k, name);
				wt(TP(np));
				pf(" %s to ", RP(np));
				wt(typ);
			} break;
			case O_COMPARE: {
				const char *op;
				switch (ins->operation.compare_type) {
				case O_EQ: op = "icmp eq"; break;
				case O_GT: op = "icmp ugt"; break; // TODO: check signs
				case O_GTE: op = "icmp uge"; break;
				case O_LT: op = "icmp ult"; break;
				case O_LTE: op = "icmp ule"; break;
				case O_NE: op = "icmp ne"; break;
				}
				pf("  %%n%zu = %s ", k, op);
				wt(TP(0));
				pf(" %s, %s", RP(0), RP(1));
			} break;
			case O_GET_FIELD:
				pf("  %%temp.%zu = getelementptr inbounds ", k);
//				wt(TP(0));
//				pf(", ");
				wt(TP(0));
				pf(" %s, i64 0, i32 %zu\n", RP(0), ins->parameters[1]);

				pf("  %%n%zu = load ", k);
				wt(ins->type);
				pf("* %%temp.%zu, align 1", k);
				break;
			case O_GET_INDEX:
				pf("  %%temp.%zu = getelementptr inbounds ", k);
//				wt(TP(0));
//				pf(", ");
				wt(TP(0));
				pf(" %s, i64 %%n%zu\n", RP(0), ins->parameters[1]);

				pf("  %%n%zu = load ", k);
				wt(ins->type);
				pf("* %%temp.%zu, align 1", k);
				break;
			case O_GET_SYMBOL:
				// TODO: fix
				pf("  %%n%zu = %s", k, RP(0));
				break;
			case O_IDENTITY:
				fprintf(stderr, "this backend does not support identity checking\n");
				exit(1);
			case O_INSTANCEOF:
				fprintf(stderr, "this backend does not support instanceof checking\n");
				exit(1);
			case O_LITERAL:
				switch (ins->type->type) {
				case T_ARRAY:
				case T_BLOCKREF:
				case T_F32:
				case T_F64:
				case T_F128:
				case T_REF:
				case T_S8:
				case T_S16:
				case T_S32:
				case T_S64:
				case T_VOID:
				default:
					abort();
				case T_BOOL:
					vf(ref[k], ins->value_bool ? "true" : "false");
					break;
				case T_OBJECT:
					vf(ref[k], "null");
					break;
				case T_STRING: {
					char *tmp = malloc(strlen(ins->value_string) * 3 + 4);
					strcpy(tmp, "c\"");
					size_t ti = strlen(tmp);
					for (char *s = ins->value_string; *s; s++) {
						char c = *s;
						if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
							tmp[ti++] = c;
						} else {
							sprintf(tmp + ti, "\\%02hhx", c); // 3 characters added
							ti += 3;
						}
					}
					strcat(tmp, "\"");
					vfr(ref[k], tmp);
				} break;
				case T_U8:
					vf(ref[k], "%hhu", ins->value_u8);
					break;
				case T_U16:
					vf(ref[k], "%hu", ins->value_u16);
					break;
				case T_U32:
					vf(ref[k], "%u", ins->value_u32);
					break;
				case T_U64:
					vf(ref[k], "%lu", ins->value_u64);
					break;
				}
				break;
			case O_LOGIC: {
				pf("  %%a.%zu = icmp ne ", k);
				wt(TP(0));
				pf(" %s, 0\n", RP(0));

				pf("  %%b.%zu = icmp ne ", k);
				wt(TP(1));
				pf(" %s, 0\n", RP(1));

				pf("  %%n%zu = ", k);
				switch (ins->operation.logic_type) {
				case O_AND:
					pf("and");
					break;
				case O_OR:
					pf("or");
					break;
				case O_XOR:
					pf("xor");
					break;
				default:
					abort();
				}
				pf(" i1 %%a.%zu, %%b.%zu", k, k);
			} break;
			case O_NEGATE:
				pf("  %%n%zu = sub ", k);
				wt(ins->type);
				pf(" 0, %s", RP(0));
				break;
			case O_NEW:
				pf("  %%zptr.%zu = inttoptr i32 0 to %%struct.%zu*\n", k, ins->type->struct_index);
				pf("  %%psize.%zu = getelementptr " /* "%%struct.%zu, " */ "%%struct.%zu* %%zptr.%zu, i64 1\n", k, /* ins->type->struct_index, */ ins->type->struct_index, k);
				pf("  %%size.%zu = ptrtoint %%struct.%zu* %%psize.%zu to i64\n", k, ins->type->struct_index, k); // TODO: is i64 too long for 32-bit?
				pf("  %%raw.%zu = call ccc i8* @allocate(i64 %%size.%zu)\n", k, k); // TODO: garbage collection
				pf("  %%n%zu = bitcast i8* %%raw.%zu to %%struct.%zu*", k, k, ins->type->struct_index);
				break;
			case O_NOT:
				pf("  %%n%zu = icmp eq ", k);
				wt(TP(0));
				pf(" %s, 0", RP(0));
				break;
			case O_NUMERIC: {
				const char *op;
				switch (ins->operation.numeric_type) {
				case O_ADD: op = "add"; break;
				case O_BAND: op = "and"; break;
				case O_BOR: op = "or"; break;
				case O_BXOR: op = "xor"; break;
				case O_DIV: op = "udiv"; break; // TODO: check signs
				case O_MOD: op = "urem"; break; // TODO: check signs
				case O_MUL: op = "mul"; break;
				case O_SUB: op = "sub"; break;
				default: abort();
				}
				pf("  %%n%zu = %s ", k, op);
				wt(ins->type);
				pf(" %s, %s", RP(0), RP(1));
			} break;
			case O_SET_FIELD:
				pf("  %%temp.%zu = getelementptr inbounds ", k);
				type *t = TP(0);
				if (t->type != T_OBJECT) {
					fprintf(stderr, "expected object in SET_FIELD");
					abort();
				}
				wt(t);
				pf(" %s, i64 0, i32 %zu\n", RP(0), ins->parameters[1]);

				type *ft = system->structs[t->struct_index].fields[ins->parameters[1]].field_type;

				pf("  store ");
				wt(ft);
				pf(" %s, ", RP(2));
				wt(ft);
				pf("* %%temp.%zu, align 1", k);
				break;
			case O_SET_INDEX:
				pf("  %%temp.%zu = getelementptr inbounds ", k);
				wt(TP(0));
				pf(" %s, i64 %%n%zu\n", RP(0), ins->parameters[1]);

				type *et = t->arraytype;

				pf("  store ");
				wt(et);
				pf(" %s, ", RP(2));
				wt(et);
				pf("* %%temp.%zu, align 1", k);
				break;
			case O_SHIFT: {
				const char *op;
				switch (ins->operation.shift_type) {
				case O_LSHIFT:
					op = "shl";
					break;
				case O_ASHIFT:
					op = "ashr";
					break;
				case O_RSHIFT:
					op = "lshr";
					break;
				default:
					abort();
				}
				pf("  %%n%zu = %s ", k, op);
				wt(ins->type);
				pf(" %s, %s", RP(0), RP(1));
			} break;
			case O_STR_CONCAT:
				fprintf(stderr, "this backend does not support string concatenation\n");
				exit(1);
			case O_CALL:
			case O_FUNCTION:
			case O_NUMERIC_ASSIGN:
			case O_POSTFIX:
			case O_SET_SYMBOL:
			case O_SHIFT_ASSIGN:
			case O_STR_CONCAT_ASSIGN:
			case O_TERNARY:
				abort();
			}
			pf("\n");
		}

		if (block->is_final) {
			pf("  call ccc void @exit(i32 0) noreturn\n");
		} else {
			switch (block->tail.type) {
			case GOTO: {
				pf("  tail call fastcc void %s(", pt_fetch(ref[block->tail.first_block]));
				size_t params = block->tail.parameter_count;
				if (params) {
					wt(TYPEOF(block->tail.parameters[0]));
					pf(" %s", pt_fetch(ref[block->tail.parameters[0]]));

					for (size_t i = 1; i < params; i++) {
						pf(", ");
						wt(TYPEOF(block->tail.parameters[i]));
						pf(" %s", pt_fetch(ref[block->tail.parameters[i]]));
					}
				}
				pf(") noreturn\n");
			} break;
			case BRANCH: {
				pf("  br i1 %s, label %%iftrue, label %%iffalse\n", pt_fetch(ref[block->tail.condition]));
				pf("iftrue:\n");
				pf("  tail call fastcc void %s(", pt_fetch(ref[block->tail.first_block]));
				size_t params = block->tail.parameter_count; // TODO: less code duplication
				if (params) {
					wt(TYPEOF(block->tail.parameters[0]));
					pf(" %s", pt_fetch(ref[block->tail.parameters[0]]));

					for (size_t i = 1; i < params; i++) {
						pf(", ");
						wt(TYPEOF(block->tail.parameters[i]));
						pf(" %s", pt_fetch(ref[block->tail.parameters[i]]));
					}
				}
				pf(") noreturn\n");
				pf("  ret void\n");

				pf("iffalse:\n");
				pf("  tail call fastcc void %s(", pt_fetch(ref[block->tail.second_block]));
				if (params) {
					wt(TYPEOF(block->tail.parameters[0]));
					pf(" %s", pt_fetch(ref[block->tail.parameters[0]]));

					for (size_t i = 1; i < params; i++) {
						pf(", ");
						wt(TYPEOF(block->tail.parameters[i]));
						pf(" %s", pt_fetch(ref[block->tail.parameters[i]]));
					}
				}
				pf(") noreturn\n");
				pf("  ret void\n");
			} break;
			}
		}

		pf("  ret void\n}\n");
	}

	pf("\n\n");
	pf("attributes #0 = { inlinehint noreturn nounwind }\n");

	pt_finalize(out);
}
