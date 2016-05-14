#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../backend.h"
#include "patch.h"
#include "types.h"

#define vf(var, ...) { char *cptr; if (asprintf(&cptr, __VA_ARGS__) == -1) { perror("asprintf"); exit(1); } pt_put(var, cptr); }
#define vfr(var, x) { pt_put(var, x); }

static struct llvm_type *convert_type(type *t) {
	if (t == NULL) {
		abort();
	}
	switch (t->type) {
	case T_ARRAY:
		fprintf(stderr, "array type not implemented\n");
		exit(1);
	case T_BLOCKREF:
		return LL_BLOCK;
	case T_BOOL:
		return LL_I1;
	case T_F32:
		return LL_F32;
	case T_F64:
		return LL_F64;
	case T_F128:
		return LL_F128;
	case T_OBJECT:
		WRONG AND BAD.
		return LL_OBJ(t->struct_index);
	case T_REF:
		abort();
	case T_U8:
	case T_S8:
		// in LLVM, signedness is distinguished on the instructions, not the types
		return LL_I8;
	case T_U16:
	case T_S16:
		return LL_I16;
	case T_U32:
	case T_S32:
		return LL_I32;
	case T_U64:
	case T_S64:
		return LL_I64;
	case T_STRING:
		// Despite what this looks like, it's actually not a C string.
		// It's a length-prefixed string: the length is stored as the first 64 bits (sans the first bit)
		// The length counts the number of bytes following those 64 bits.
		// There is no null terminator.
		// The first bit is set to 1 if the string is statically-allocated instead of heap-allocated.
		return LL_PTR(LL_I8);
	case T_VOID:
		return LL_VOID;
	default:
		abort();
	}
}

static void wt(type *t) {
	char *ts = llvm_type_string(convert_type(t), LTS_DEALLOC);
	pt_printf("%s", ts);
}

bool check_prototypes(code_block *from, code_block *to, size_t toindex) {
	if (from->is_final || from->tail.parameter_count != to->parameter_count) {
		return false;
	}
	switch (from->tail.type) {
	case GOTO:
		if (from->tail.first_block >= from->parameter_count
		 && from->instructions[from->tail.first_block - from->parameter_count].operation.type == O_BLOCKREF) {
			return from->instructions[from->tail.first_block - from->parameter_count].block_index == toindex;
		}
		break;
	case BRANCH:
		if (from->tail.first_block >= from->parameter_count && from->tail.second_block >= from->parameter_count
		 && from->instructions[from->tail.first_block - from->parameter_count].operation.type == O_BLOCKREF
		 && from->instructions[from->tail.second_block - from->parameter_count].operation.type == O_BLOCKREF) {
			return from->instructions[from->tail.first_block - from->parameter_count].block_index == toindex
			    || from->instructions[from->tail.second_block - from->parameter_count].block_index == toindex;
		}
		break;
	}

	for (size_t p = 0; p < to->parameter_count; p++) {
		type *req = to->parameters[p].field_type;
		size_t iind = from->tail.parameters[p];
		type *found = iind < from->parameter_count ? from->parameters[iind].field_type : from->instructions[iind - from->parameter_count].type;
		if (!equivalent_type(req, found)) {
			return false;
		}
	}
	return true;
}

#define IS_GC_ABLE(tp) ((tp) != NULL && ((tp)->type == T_OBJECT || (tp)->type == T_ARRAY || (tp)->type == T_STRING))

void backend_write(code_system *system, FILE *out) {
	pt_reset();

	pt_printf("target datalayout = \"e-m:e-i64:64-f80:128-n8:16:32:64-S128\"\n"
		 "target triple = \"x86_64-unknown-linux-gnu\"\n\n\n");

	// metafield: linked list: offset of field, is field a string, next
	pt_printf("%%metafield = type { i32, i8, %%metafield* }\n");

	// metastruct: OBJECT_LENGTH, STRUCT_ID, metanode*
	pt_printf("%%metastruct = type { i32, i32, %%metafield* }\n");

	if (system->struct_count) {
		for (size_t i = 0; i < system->struct_count; i++) {
			code_struct *str = get_code_struct(system, i);

			size_t mf_count = 0;
			for (size_t j = 0; j < str->field_count; j++) {
				type *type = str->fields[j].field_type;
				if (IS_GC_ABLE(type)) {
					mf_count++;
				}
			}
			size_t mfi = 0;
			pt_printf("%%struct.%zu = type { ", i);
			for (size_t j = 0; j < str->field_count; j++) {
				if (j != 0) {
					pt_printf(", ");
				}
				wt(str->fields[j].field_type);
			}
			pt_printf(" }\n");
			pt_printf("; types: ");
			for (size_t j = 0; j < str->field_count; j++) {
				pt_printf("%u ", str->fields[j].field_type->type);
			}
			pt_printf("\n")
			for (size_t j = 0; j < str->field_count; j++) {
				type *type = str->fields[j].field_type;
				if (IS_GC_ABLE(type)) {
					pt_printf("@mf.%zu_%zu = unnamed_addr constant %%metafield { i32 ptrtoint(", i, mfi);
					wt(type);
					pt_printf("* getelementptr(%%struct.%zu, %%struct.%zu* inttoptr(i32 0 to %%struct.%zu*), i64 0, i32 %zu) to i32), ", i, i, i, j);
					if (type->type == T_OBJECT) {
						pt_printf("i8 0, ");
					} else if (type->type == T_STRING) {
						pt_printf("i8 1, ");
					} else {
						abort();
					}
					mfi++;
					if (mfi == mf_count) { // we're the last
						pt_printf("%%metafield* null }\n");
					} else {
						pt_printf("%%metafield* @mf.%zu_%zu }\n", i, mfi);
					}
				}
			}
			pt_printf("@meta.%zu = unnamed_addr constant %%metastruct { i32 ", i);
			pt_printf("ptrtoint(%%struct.%zu* getelementptr(%%struct.%zu, %%struct.%zu* inttoptr(i32 0 to %%struct.%zu*), i64 1) to i32), ", i, i, i, i);
			pt_printf("i32 %zu, ", i);
			if (mf_count == 0) {
				pt_printf("%%metafield* null }\n");
			} else {
				pt_printf("%%metafield* @mf.%zu_0 }\n", i);
			}
		}
		pt_printf("\n\n");
	}

	pt_printf("declare i8* @bear_new(%%metastruct*, i32, i8*) nounwind\n");
	pt_printf("declare i1 @bear_streq(i8*, i8*) nounwind\n");

	pt_printf("declare i8* @llvm.stacksave()\n");
	pt_printf("declare void @llvm.stackrestore(i8* %%ptr)\n\n");

	size_t natid = 0, natcap = 10;
	char **natives = malloc(natcap * sizeof(char*));
	if (natives == NULL) {
		fputs("alloc failed\n", stderr);
		exit(1);
	}

#define TYPEOF(x) ((x) < block->parameter_count ? block->parameters[x].field_type : block->instructions[x - block->parameter_count].type)

	// constant and prototype extraction
	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = get_code_block(system, i);
		for (size_t j = 0; j < block->instruction_count; j++) {
			code_instruction *insr = &block->instructions[j];
			if (insr->operation.type == O_NATIVE) {
				char *name = insr->native_call;
				bool found = false;
				for (size_t k = 0; k < natid; k++) {
					if (strcmp(name, natives[k]) == 0) {
						found = true;
						break;
					}
				}
				if (!found) {
					if (natid >= natcap) {
						natives = realloc(natives, (natcap <<= 1) * sizeof(char*));
					}
					natives[natid++] = name;
					pt_printf("declare ");
					wt(insr->type);
					pt_printf(" @%s(", name);
					size_t count = insr->parameters[0];
					if (count) {
						wt(TYPEOF(insr->parameters[1]));
						for (size_t k = 2; k <= count; k++) {
							pt_printf(", ");
							wt(TYPEOF(insr->parameters[k]));
						}
					}
					pt_printf(") nounwind\n")
				}
			} else if (insr->operation.type == O_LITERAL && insr->type->type == T_STRING) {
				pt_printf("@str_%zu_%zu = private unnamed_addr constant [%zu x i8] c\"", i, j, strlen(insr->value_string) + 8);
				// TODO: check endianness
				uint64_t len = strlen(insr->value_string) | 0x8000000000000000; // specify that it's statically-allocated.
				pt_printf("\\%02hhx\\%02hhx\\%02hhx\\%02hhx\\%02hhx\\%02hhx\\%02hhx\\%02hhx", (uint8_t) len, (uint8_t) (len >> 8), (uint8_t) (len >> 16), (uint8_t) (len >> 24), (uint8_t) (len >> 32), (uint8_t) (len >> 40), (uint8_t) (len >> 48), (uint8_t) (len >> 56));
				for (char *s = insr->value_string; *s; s++) {
					char c = *s;
					if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
						pt_printf("%c", c);
					} else {
						pt_printf("\\%02hhx", c);
					}
				}
				pt_printf("\"\n");
			}
		}
	}

	pt_printf("\ndefine i32 @main() gc \"\" {\n");
	pt_printf("  br label %%Block0\n");

	struct patchvar **allrefs[system->block_count];

	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = get_code_block(system, i);
		size_t len = block->parameter_count + block->instruction_count;
		allrefs[i] = malloc(sizeof(struct patchvar *) * len);
		if (allrefs[i] == NULL) {
			fputs("alloc failed\n", stderr);
			exit(1);
		}
		for (size_t j = 0; j < len; j++) {
			allrefs[i][j] = pt_def();
		}
	}

	bool possibly_accessible[system->block_count];
	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = get_code_block(system, i);

		bool any_possible_sources = false;
		if (i == 0) {
			any_possible_sources = true;
		} else {
			for (size_t k = 0; k < system->block_count; k++) {
				if (check_prototypes(get_code_block(system, k), block, i)) {
					any_possible_sources = true;
					break;
				}
			}
		}

		possibly_accessible[i] = any_possible_sources;
	}

	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = get_code_block(system, i);
		size_t offset = block->parameter_count;

		struct patchvar **ref = allrefs[i];

#define RP(n) pt_fetch(ref[ins->parameters[(n)]])
#define TP(n) (TYPEOF(ins->parameters[(n)]))

		for (size_t k = 0; k < block->parameter_count + block->instruction_count; k++) {
			vf(ref[k], "%%b%zu_%zu", i, k);
		}

#define SET(x, ...) pt_printf("  %%b%zu_%zu = " x, i, k, __VA_ARGS__)
#define SETR(x) SET("%s", x)

		if (!possibly_accessible[i]) {
			continue;
		}

		pt_printf("Block%zu:\n", i);
		// parameters via PHI nodes
		for (size_t k = 0; k < offset; k++) {
			SETR("phi ");
			wt(block->parameters[k].field_type);
			bool first = true;
			for (size_t l = 0; l < system->block_count; l++) {
				code_block *from = get_code_block(system, l);
				// we want to limit the number of source possibilities - so we make sure the prototype matches.
				if (possibly_accessible[l] && check_prototypes(from, block, i)) {
					size_t sourceid = from->tail.parameters[k];
					if (first) {
						first = false;
					} else {
						pt_printf(",");
					}
					pt_printf(" [ ")
					pt_use(allrefs[l][sourceid]); // this works despite the possibility of the ref changing over the course of a block because other blocks only see the final refs
					pt_printf(", %%Block%zu ]", l);
				}
			}
			pt_printf("\n");
		}

		size_t last_used_map[block->parameter_count + block->instruction_count];
		for (size_t j = 0; j < block->parameter_count; j++) {
			last_used_map[j] = -1;
		}
		for (size_t j = 0; j < block->instruction_count; j++) {
			last_used_map[block->parameter_count + j] = j;
		}
		if (!block->is_final) {
			switch (block->tail.type) {
			case GOTO:
				last_used_map[block->tail.first_block] = block->instruction_count;
				break;
			case BRANCH:
				last_used_map[block->tail.first_block] = block->instruction_count;
				last_used_map[block->tail.second_block] = block->instruction_count;
				break;
			}
		}
		for (size_t j = 0; j < block->instruction_count; j++) {
			code_instruction *ins = &block->instructions[j];

#define ULP(x) last_used_map[ins->parameters[x]] = j;

			switch (ins->operation.type) {
			case O_LITERAL:
			case O_BLOCKREF:
			case O_NEW:
			case O_NEW_ARRAY:
				break;
			case O_BITWISE_NOT:
			case O_GET_FIELD:
			case O_GET_SYMBOL:
			case O_NEGATE:
			case O_NOT:
				ULP(0)
				break;
			case O_CAST:
				ULP(0);
				break;
			case O_COMPARE:
			case O_GET_INDEX:
			case O_NUMERIC:
			case O_LOGIC:
			case O_SHIFT:
				ULP(0);
				ULP(1);
				break;
			case O_SET_FIELD:
				ULP(0);
				ULP(2);
				break;
			case O_SET_INDEX:
				ULP(0);
				ULP(1);
				ULP(2);
				break;
			case O_NATIVE: {
				size_t count = ins->parameters[0];
				for (size_t i = 1; i <= count; i++) {
					ULP(i);
				}
			} break;
			case O_IDENTITY:
			case O_INSTANCEOF:
			case O_STR_CONCAT:
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
		}

		for (size_t j = 0; j < block->instruction_count; j++) {
			size_t k = j + offset;
			code_instruction *ins = &block->instructions[j];

#ifdef TRACE_EXECUTION
			pt_printf("    call void @bear_print_number(i64 %zu)\n", 10000 * i + 100 * j);
#endif

			switch (ins->operation.type) {
			case O_BITWISE_NOT:
				SETR("xor ");
				wt(ins->type);
				pt_printf(" %s, -1", RP(0));
				break;
			case O_BLOCKREF:
				vf(ref[k], "blockaddress(@main, %%Block%zu)", ins->block_index);
				break;
			case O_CAST: {
				const char *name = NULL;
				switch (ins->operation.cast_type) {
				case O_UPCAST:
				case O_DOWNCAST: // TODO: check downcasts
					name = "bitcast";
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
				SET("%s ", name);
				wt(TP(0));
				pt_printf(" %s to ", RP(0));
				wt(ins->type);
			} break;
			case O_COMPARE: {
				const char *op;
				type *left = TP(0), *right = TP(1);
				if (left->type != right->type) {
					fputs("comparison type mismatch\n", stderr);
					exit(1);
				}
				bool is_signed = false, done = false;
				switch (left->type) {
				case T_S8:
				case T_S16:
				case T_S32:
				case T_S64:
					is_signed = true;
				case T_U8:
				case T_U16:
				case T_U32:
				case T_U64:
				case T_BOOL:
					switch (ins->operation.compare_type) {
					case O_EQ: op = "icmp eq"; break;
					case O_GT: op = is_signed ? "icmp sgt" : "icmp ugt"; break;
					case O_GTE: op = is_signed ? "icmp sge" : "icmp uge"; break;
					case O_LT: op = is_signed ? "icmp slt" : "icmp ult"; break;
					case O_LTE: op = is_signed ? "icmp sle" : "icmp ule"; break;
					case O_NE: op = "icmp ne"; break;
					}
					break;
				case T_F32:
				case T_F64:
				case T_F128:
					switch (ins->operation.compare_type) {
					case O_EQ: op = "fcmp oeq"; break;
					case O_GT: op = "fcmp ogt"; break;
					case O_GTE: op = "fcmp oge"; break;
					case O_LT: op = "fcmp olt"; break;
					case O_LTE: op = "fcmp ole"; break;
					case O_NE: op = "fcmp one"; break;
					}
					break;
				case T_OBJECT:
				case T_BLOCKREF:
					switch (ins->operation.compare_type) {
					case O_EQ: op = "icmp eq"; break;
					case O_NE: op = "icmp ne"; break;
					case O_GT:
					case O_GTE:
					case O_LT:
					case O_LTE:
						abort();
					}
					break;
				case T_STRING:
					switch (ins->operation.compare_type) {
					case O_EQ:
						SET("call i1 @bear_streq(i8* %s, i8* %s)", RP(0), RP(1));
						done = true;
						break;
					case O_NE:
						pt_printf("  %%temp.%zu_%zu = call i1 @bear_streq(i8* %s, i8* %s)\n", i, k, RP(0), RP(1));
						SET("icmp eq i1 %%temp.%zu_%zu, 0", i, k);
						done = true;
						break;
					case O_GT:
					case O_GTE:
					case O_LT:
					case O_LTE:
						abort();
					}
					break;
				default:
					fputs("unsupported type to compare\n", stderr);
					exit(1);
				}
				if (!done) {
					SET("%s ", op);
					wt(left);
					pt_printf(" %s, %s", RP(0), RP(1));
				}
			} break;
			case O_GET_FIELD:
				pt_printf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
				assert(TP(0)->type == T_OBJECT);
				pt_printf("%%struct.%zu, ", TP(0)->struct_index);
				wt(TP(0));
				pt_printf(" %s, i64 0, i32 %zu\n", RP(0), ins->parameters[1]);

				SETR("load ");
				wt(ins->type);
				pt_printf(", ");
				wt(ins->type);
				pt_printf("* %%temp.%zu_%zu, align 1", i, k);
				break;
			case O_GET_INDEX:
				pt_printf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
				assert(TP(0)->type == T_OBJECT);
				pt_printf("%%struct.%zu, ", TP(0)->struct_index);
				wt(TP(0));
				pt_printf(" %s, i64 %s\n", RP(0), RP(1));

				SETR("load ");
				wt(ins->type);
				pt_printf(", ");
				wt(ins->type);
				pt_printf("* %%temp.%zu_%zu, align 1", i, k);
				break;
			case O_GET_SYMBOL:
				// TODO: fix
				SET("%s", RP(0));
				break;
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
					size_t len = strlen(ins->value_string) + 8;
					SET("getelementptr [%zu x i8], [%zu x i8]* @str_%zu_%zu, i64 0, i64 0", len, len, i, j);
					break;
				}
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
				pt_printf("  %%a.%zu = icmp ne ", k);
				wt(TP(0));
				pt_printf(" %s, 0\n", RP(0));

				pt_printf("  %%b.%zu = icmp ne ", k);
				wt(TP(1));
				pt_printf(" %s, 0\n", RP(1));

				switch (ins->operation.logic_type) {
				case O_AND:
					SETR("and");
					break;
				case O_OR:
					SETR("or");
					break;
				case O_XOR:
					SETR("xor");
					break;
				default:
					abort();
				}
				pt_printf(" i1 %%a.%zu, %%b.%zu", k, k);
			} break;
			case O_NATIVE: {
				if (is_void(ins->type)) {
					pt_printf("  call void");
				} else {
					SETR("call ");
					wt(ins->type);
				}
				pt_printf(" @%s(", ins->native_call);
				size_t count = ins->parameters[0];
				if (count) {
					wt(TP(1));
					pt_printf(" %s", RP(1));
					for (size_t i = 2; i <= count; i++) {
						pt_printf(", ");
						wt(TP(i));
						pt_printf(" %s", RP(i));
					}
				}
				pt_printf(")");
			} break;
			case O_NEGATE:
				SETR("sub ");
				wt(ins->type);
				pt_printf(" 0, %s", RP(0));
				break;
			case O_NEW: { // TODO: check with GC before save-restoring to see if we need to.
				struct patchvar *strref = pt_def();
				size_t refcnt = 0;
				for (size_t ssa = 0; ssa < offset + block->instruction_count; ssa++) { // TODO: deduplicate
					size_t start = ssa - offset;
					size_t end = last_used_map[ssa];
					if ((ssa < offset || start < j) && end > j) { // might be relocated
						type *tp = TYPEOF(ssa);
						if (IS_GC_ABLE(tp) && (ssa < offset || block->instructions[ssa - offset].operation.type != O_LITERAL)) {
							// TODO: make sure that string literals aren't saved
							if (refcnt == 0) {
								pt_printf("  %%save.%zu_%zu = call i8* @llvm.stacksave()\n", i, k);
								pt_printf("  %%pass.%zu_%zu = alloca ", i, k);
								pt_use(strref);
								pt_printf("\n");
							}
							pt_printf("  ; save %zu (%zu - %zu - %zu)\n", ssa, start, j, end);

							// address of saved pointer
							pt_printf("  %%prestore.%zu_%zu_%zu = getelementptr ", i, k, refcnt);
							pt_use(strref);
							pt_printf(", ");
							pt_use(strref);
							pt_printf("* %%pass.%zu_%zu, i64 0, i32 %zu\n", i, k, refcnt);

							if (tp->type == T_STRING) {
								pt_printf("  %%int.%zu_%zu_%zu = ptrtoint ", i, k, refcnt);
								wt(tp);
								pt_printf(" %s to i64\n", pt_fetch(ref[ssa]));

								pt_printf("  %%ord.%zu_%zu_%zu = or i64 %%int.%zu_%zu_%zu, 1\n", i, k, refcnt, i, k, refcnt);
								pt_printf("  %%cast.%zu_%zu_%zu = inttoptr i64 %%ord.%zu_%zu_%zu to i8*\n", i, k, refcnt, i, k, refcnt)
							} else {
								pt_printf("  %%cast.%zu_%zu_%zu = bitcast ", i, k, refcnt);
								wt(tp);
								pt_printf(" %s to i8*\n", pt_fetch(ref[ssa]));
							}

							pt_printf("  store i8* %%cast.%zu_%zu_%zu, i8** %%prestore.%zu_%zu_%zu\n", i, k, refcnt, i, k, refcnt);
							refcnt++;
						}
					}
				}
				if (refcnt) {
					const char *single = ", i8*";
					size_t siz = strlen(single);
					char cr[siz * refcnt + 1];
					for (size_t i = 0; i < refcnt; i++) {
						strcpy(cr + siz * i, single);
					}
//					printf("INITALIZING %zu\n", i);
					vf(strref, "{ %s }", cr + 2); // add two to get rid of leading ", "
					pt_printf("\n");
				} else {
//					printf("NOT INITALIZING %zu, %zu\n", i, refcnt);
				}
				pt_printf("  %%passi8.%zu_%zu = bitcast ", i, k);
				if (refcnt) {
					pt_use(strref);
					pt_printf("* %%pass.%zu_%zu to i8*\n", i, k);
				} else {
					pt_printf("i8* null to i8*\n");
				}
				pt_printf("  %%raw.%zu_%zu = call ccc i8* @bear_new(%%metastruct* @meta.%zu, i32 %zu, i8* %%passi8.%zu_%zu )\n", i, k, ins->type->struct_index, refcnt, i, k);
				SET("bitcast i8* %%raw.%zu_%zu to ", i, k);
				wt(ins->type);
				pt_printf("\n");

				if (refcnt) {
					refcnt = 0;
					for (size_t ssa = 0; ssa < offset + block->instruction_count; ssa++) { // TODO: deduplicate
						size_t start = ssa - offset;
						size_t end = last_used_map[ssa];
						type *tp = TYPEOF(ssa);
						if ((ssa < offset || start < j) && end > j) { // might be relocated
							if (IS_GC_ABLE(tp) && (ssa < offset || block->instructions[ssa - offset].operation.type != O_LITERAL)) {
								// TODO: make sure that string literals aren't saved
								pt_printf("\n  ; restore %zu (%zu - %zu - %zu)\n", ssa, start, j, end);
								pt_printf("  %%postload.%zu_%zu_%zu = load i8*, i8** %%prestore.%zu_%zu_%zu\n", i, k, refcnt, i, k, refcnt);
								pt_printf("  %%restored.%zu_%zu.%zu = bitcast i8* %%postload.%zu_%zu_%zu to ", i, k, ssa, i, k, refcnt);
								wt(tp);
								pt_printf("\n")
								vf(ref[ssa], "%%restored.%zu_%zu.%zu", i, k, ssa);
								refcnt++;
							}
						} else if (start < j && IS_GC_ABLE(tp)) {
							pt_printf("  ; NOTE: busting %zu (%zu - %zu - %zu)\n", ssa, start, j, end);
							vf(ref[ssa], "BUSTED"); // not moved forward. if this ever shows up, then maybe it should have been.
						}
					}
					pt_printf("  call void @llvm.stackrestore(i8* %%save.%zu_%zu)\n", i, k);
				}
				// ins->parameters[0] is the length of O_NEW_ARRAY
			} break;
			case O_NOT:
				SETR("icmp eq ");
				wt(TP(0));
				pt_printf(" %s, 0", RP(0));
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
				SET("%s ", op);
				wt(ins->type);
				pt_printf(" %s, %s", RP(0), RP(1));
			} break;
			case O_SET_FIELD:
				pt_printf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
				type *t = TP(0);
				if (t->type != T_OBJECT) {
					fprintf(stderr, "expected object in SET_FIELD");
					abort();
				}
				pt_printf("%%struct.%zu, ", t->struct_index);
				wt(t);
				pt_printf(" %s, i64 0, i32 %zu\n", RP(0), ins->parameters[1]);

				type *ft = get_code_struct(system, t->struct_index)->fields[ins->parameters[1]].field_type;

				pt_printf("  store ");
				wt(ft);
				pt_printf(" %s, ", RP(2));
				wt(ft);
				pt_printf("* %%temp.%zu_%zu, align 1", i, k);
				break;
			case O_SET_INDEX:
				pt_printf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
				if (t->type != T_ARRAY) {
					fprintf(stderr, "expected array in SET_INDEX");
					abort();
				}
				wt(TP(0));
				pt_printf(" (needs to have the star stripped), "); // TODO
				wt(TP(0));
				pt_printf(" %s, i64 %s\n", RP(0), RP(1));

				type *et = t->arraytype;

				pt_printf("  store ");
				wt(et);
				pt_printf(" %s, ", RP(2));
				wt(et);
				pt_printf("* %%temp.%zu_%zu, align 1", i, k);
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
				SET("%s ", op);
				wt(ins->type);
				pt_printf(" %s, %s", RP(0), RP(1));
			} break;
			case O_NEW_ARRAY:
			case O_STR_CONCAT:
			case O_IDENTITY:
			case O_INSTANCEOF:
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
			pt_printf("\n");
		}

		if (block->is_final) {
			pt_printf("  br label %%Done\n");
		} else {
			bool needs_indirection = true;
			switch (block->tail.type) {
			case GOTO: {
				if (block->tail.first_block >= offset
				 && block->instructions[block->tail.first_block - offset].operation.type == O_BLOCKREF) {
					pt_printf("  br label %%Block%zu\n", block->instructions[block->tail.first_block - offset].block_index);
					needs_indirection = false;
				} else {
					pt_printf("  indirectbr i8* %s, [ ", pt_fetch(ref[block->tail.first_block]));
				}
			} break;
			case BRANCH: {
				if (block->tail.first_block >= offset
				 && block->tail.second_block >= offset
				 && block->instructions[block->tail.first_block - offset].operation.type == O_BLOCKREF
				 && block->instructions[block->tail.second_block - offset].operation.type == O_BLOCKREF) {
					pt_printf("  br i1 %s, label %%Block%zu, label %%Block%zu\n",
					   pt_fetch(ref[block->tail.condition]),
					   block->instructions[block->tail.first_block - offset].block_index,
					   block->instructions[block->tail.second_block - offset].block_index);
					needs_indirection = false;
				} else {
					pt_printf("  %%brtarget = select i1 %s, i8* %s, i8* %s\n",
					   pt_fetch(ref[block->tail.condition]),
					   pt_fetch(ref[block->tail.first_block]),
					   pt_fetch(ref[block->tail.second_block]));
					pt_printf("  indirectbr i8* %%brtarget, [ ");
				}
			} break;
			}
			if (needs_indirection) {
				bool first = true;
				for (size_t j = 0; j < system->block_count; j++) {
					if (check_prototypes(block, get_code_block(system, j), j)) {
						if (first) {
							first = false;
						} else {
							pt_printf(", ");
						}
						pt_printf("label %%Block%zu", j);
					}
				}
				pt_printf(" ]\n");
			}
		}
	}

	pt_printf("Done:\n  ret i32 0\n}\n");

	for (size_t i = 0; i < system->block_count; i++) {
		free(allrefs[i]);
	}

	pt_finalize(out);
}
