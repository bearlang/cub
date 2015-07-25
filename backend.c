#include <math.h>

#include "xalloc.h"
#include "backend.h"

static void w(FILE *file, char *string) {
  fputs(string, file);
}

static void wb(FILE *file, char *bytes, size_t size) {
  fwrite(bytes, sizeof(char), size, file);
}

#define wbin(file, info, index, op) do {\
  wt(file, info->type, "instruction", index, true);\
  wf(file, " = ");\
  wf(file, "instruction_");\
  wi(file, info->parameters[0]);\
  w(file, op);\
  wf(file, "instruction_");\
  wi(file, info->parameters[1]);\
} while (0);

#define wbinf(file, info, index, op) do {\
  wt(file, info->type, "instruction", index, true);\
  wf(file, " = ");\
  wf(file, "instruction_");\
  wi(file, info->parameters[0]);\
  wf(file, op);\
  wf(file, "instruction_");\
  wi(file, info->parameters[1]);\
} while (0);

static void wc(FILE *file, char chr) {
  char buf[1];
  buf[0] = chr;
  wb(file, buf, 1);
}

#define wf(file, v) do {\
  fwrite(v, sizeof(char), sizeof(v) - 1, file);\
} while (0);

static void wi(FILE *file, size_t value) {
  fprintf(file, "%zu", value);
}

static void wt(FILE *file, type *t, char *name_type, size_t i, bool constant) {
  char *type = "void";
  if (t) {
    switch (t->type) {
    case T_ARRAY:
      fprintf(stderr, "array type not implemented\n");
      exit(1);
    case T_BLOCKREF:
      wf(file, "void (*");
      if (name_type) {
        w(file, name_type);
        wc(file, '_');
        wi(file, i);
      }
      wf(file, ")(");
      argument *arg = t->blocktype;
      if (arg) {
        wt(file, arg->argument_type, NULL, 0, true);
        arg = arg->next;
        for (; arg; arg = arg->next) {
          wf(file, ", ");
          wt(file, arg->argument_type, NULL, 0, true);
        }
      }
      wc(file, ')');
      return;
    case T_BOOL: type = "bool"; break;
    case T_F32: type = "float"; break;
    case T_F64: type = "double"; break;
    case T_F128: type = "long double"; break;
    case T_OBJECT:
      w(file, "struct_");
      wi(file, t->struct_index);
      wc(file, '*');
      if (name_type) {
        wc(file, ' ');
        w(file, name_type);
        wc(file, '_');
        wi(file, i);
      }
      return;
    case T_REF:
      abort();
    case T_S8: type = "int8_t"; break;
    case T_S16: type = "int16_t"; break;
    case T_S32: type = "int32_t"; break;
    case T_S64: type = "int64_t"; break;
    case T_STRING: type = "char*"; break;
    case T_U8: type = "uint8_t"; break;
    case T_U16: type = "uint16_t"; break;
    case T_U32: type = "uint32_t"; break;
    case T_U64: type = "uint64_t"; break;
    case T_VOID: type = "void"; break;
    }
  }
  if (constant) {
    wf(file, "const ");
  }
  w(file, type);
  if (name_type) {
    wc(file, ' ');
    w(file, name_type);
    wc(file, '_');
    wi(file, i);
  }
}

void backend_write(code_system *system, FILE *out) {
  wf(out,
    "#include <stdint.h>\n"
    "#include <stdlib.h>\n\n"
    "typedef enum {false, true} bool;\n");

  if (system->struct_count) {
    wc(out, '\n');

    for (size_t i = 0; i < system->struct_count; i++) {
      wf(out, "typedef struct struct_");
      wi(out, i);
      wf(out, " struct_");
      wi(out, i);
      wf(out, ";\n");
    }

    wc(out, '\n');

    for (size_t i = 0; i < system->struct_count; i++) {
      code_struct *str = &system->structs[i];

      wf(out, "\nstruct struct_");
      wi(out, i);
      wf(out, " {\n");
      for (size_t j = 0; j < str->field_count; j++) {
        code_field *field = &str->fields[j];
        wf(out, "  ");
        wt(out, field->field_type, "field", j, false);
        wf(out, ";\n");
      }
      wf(out, "};\n");
    }
  }

  wf(out, "\n\n");
  for (size_t i = 0; i < system->block_count; i++) {
    code_block *block = &system->blocks[i];

    wf(out, "static void block_");
    wi(out, i);
    wc(out, '(');

    size_t params = block->parameter_count;
    if (params) {
      code_field *field = &block->parameters[0];
      wt(out, field->field_type, NULL, 0, true);

      for (size_t j = 1; j < params; j++) {
        field = &block->parameters[j];

        wf(out, ", ");
        wt(out, field->field_type, NULL, 0, true);
      }
    }

    wf(out, ");\n");
  }

  wc(out, '\n');
  for (size_t i = 0; i < system->block_count; i++) {
    code_block *block = &system->blocks[i];

    wf(out, "\nstatic void block_");
    wi(out, i);
    wc(out, '(');

    size_t offset = block->parameter_count;
    if (offset) {
      code_field *field = &block->parameters[0];
      wt(out, field->field_type, "instruction", 0, true);

      for (size_t j = 1; j < offset; j++) {
        field = &block->parameters[j];

        wf(out, ", ");
        wt(out, field->field_type, "instruction", j, true);
      }
    }

    wf(out, ") {\n");

    for (size_t j = 0; j < block->instruction_count; j++) {
      size_t k = j + offset;
      code_instruction *ins = &block->instructions[j];
      wf(out, "  ");
      switch (ins->operation.type) {
      case O_BITWISE_NOT:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = ~instruction_");
        wi(out, ins->parameters[0]);
        break;
      case O_BLOCKREF:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = &block_");
        wi(out, ins->block_index);
        break;
      case O_CAST:
        switch (ins->operation.cast_type) {
        case O_UPCAST:
        case O_DOWNCAST:
          wt(out, ins->type, "instruction", k, true);
          wf(out, " = (");
          type t = {
            .type = T_OBJECT,
            .struct_index = ins->parameters[0]
          };
          wt(out, &t, NULL, 0, true);
          wf(out, ") instruction_");
          wi(out, ins->parameters[1]);
          break;
        case O_FLOAT_EXTEND:
        case O_FLOAT_TRUNCATE:
        case O_FLOAT_TO_SIGNED:
        case O_FLOAT_TO_UNSIGNED:
        case O_SIGNED_TO_FLOAT:
        case O_UNSIGNED_TO_FLOAT:
        case O_TRUNCATE:
        case O_SIGN_EXTEND:
        case O_ZERO_EXTEND:
          wt(out, ins->type, "instruction", k, true);
          wf(out, " = (");
          wt(out, ins->type, NULL, 0, true);
          wf(out, ") instruction_");
          wi(out, ins->parameters[0]);
          break;
        case O_REINTERPRET:
          wf(out, "union {\n    ");
          wt(out, instruction_type(block, ins->parameters[0]), "sub", 0, false);
          wf(out, ";\n    ");
          wt(out, ins->type, "sub", 1, false);
          wf(out, ";\n  } union_");
          wi(out, k);
          wf(out, ";\n  union_");
          wi(out, k);
          wf(out, ".sub0 = instruction_");
          wi(out, ins->parameters[0]);
          wf(out, ";\n  ");
          wt(out, ins->type, "instruction", k, true);
          wf(out, " = union_");
          wi(out, k);
          wf(out, ".sub1");
          break;
        }
        break;
      case O_COMPARE: {
        char *op;
        switch (ins->operation.compare_type) {
        case O_EQ: op = " == "; break;
        case O_GT: op = " > "; break;
        case O_GTE: op = " >= "; break;
        case O_LT: op = " < "; break;
        case O_LTE: op = " <= "; break;
        case O_NE: op = " != "; break;
        }
        wbin(out, ins, k, op);
      } break;
      case O_GET_FIELD:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = instruction_");
        wi(out, ins->parameters[0]);
        wf(out, "->field_");
        wi(out, ins->parameters[1]);
        break;
      case O_GET_INDEX:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = instruction_");
        wi(out, ins->parameters[0]);
        wf(out, "[instruction_");
        wi(out, ins->parameters[1]);
        wc(out, ']');
        break;
      case O_GET_SYMBOL:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = instruction_");
        wi(out, ins->parameters[0]);
        break;
      case O_IDENTITY:
        fprintf(stderr, "this backend does not support identity checking\n");
        exit(1);
      case O_INSTANCEOF:
        fprintf(stderr, "this backend does not support instanceof checking\n");
        exit(1);
      case O_LITERAL:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = ");
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
          abort();
        case T_BOOL:
          w(out, ins->value_bool ? "true" : "false");
          break;
        case T_OBJECT:
          wf(out, "null");
          break;
        case T_STRING: {
          wf(out, "(const char*) \"");
          for (char *s = ins->value_string; *s; s++) {
            fprintf(out, "\\x%02hhx", *s);
          }
          wc(out, '"');
        } break;
        case T_U8: {
          char buf[5];
          snprintf(buf, 5, "0x%02hhx", ins->value_u8);
          wf(out, buf);
        } break;
        case T_U16: {
          char buf[7];
          snprintf(buf, 7, "0x%04hx", ins->value_u16);
          wf(out, buf);
        } break;
        case T_U32: {
          char buf[11];
          snprintf(buf, 11, "0x%08x", ins->value_u32);
          wf(out, buf);
        } break;
        case T_U64: {
          char buf[19];
          snprintf(buf, 19, "0x%016lx", ins->value_u64);
          wf(out, buf);
        } break;
        }
        break;
      case O_LOGIC:
        abort();
        /*switch (ins->operation.logic_type) {
        case O_AND: wbinf(out, ins, k, " && "); break;
        case O_OR: wbinf(out, ins, k, " || "); break;
        case O_XOR: wbinf(out, ins, k, " != "); break;
        }
        break;*/
      case O_NEGATE:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = -instruction_");
        wi(out, ins->parameters[0]);
        break;
      case O_NEW:
        wf(out, "struct_");
        wi(out, ins->type->struct_index);
        wf(out, " instruction_");
        wi(out, k);
        wf(out, "_src;\n  ")
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = &instruction_");
        wi(out, k);
        wf(out, "_src");
        // awww yeah memory leaks XD
        // wf(out, " = malloc(sizeof(*instruction_");
        // wi(out, k);
        // wf(out, "))");
        break;
      case O_NOT:
        wt(out, ins->type, "instruction", k, true);
        wf(out, " = !instruction_");
        wi(out, ins->parameters[0]);
        break;
      case O_NUMERIC: {
        switch (ins->operation.numeric_type) {
        case O_ADD: wbinf(out, ins, k, " + "); break;
        case O_BAND: wbinf(out, ins, k, " & "); break;
        case O_BOR: wbinf(out, ins, k, " | "); break;
        case O_BXOR: wbinf(out, ins, k, " ^ "); break;
        case O_DIV: wbinf(out, ins, k, " / "); break;
        case O_MOD: wbinf(out, ins, k, " % "); break;
        case O_MUL: wbinf(out, ins, k, " * "); break;
        case O_SUB: wbinf(out, ins, k, " - "); break;
        }
      } break;
      case O_SET_FIELD:
        wf(out, "instruction_");
        wi(out, ins->parameters[0]);
        wf(out, "->field_");
        wi(out, ins->parameters[1]);
        wf(out, " = instruction_");
        wi(out, ins->parameters[2]);
        break;
      case O_SET_INDEX:
        wf(out, "instruction_");
        wi(out, ins->parameters[0]);
        wf(out, "[instruction_");
        wi(out, ins->parameters[1]);
        wf(out, "] = instruction_");
        wi(out, ins->parameters[2]);
        break;
      case O_SHIFT:
        switch (ins->operation.shift_type) {
        case O_LSHIFT:
          wbinf(out, ins, k, " << ");
          break;
        case O_ASHIFT:
        case O_RSHIFT: {
          type *left_type = instruction_type(block, ins->parameters[0]);
          wt(out, ins->type, "instruction", k, true);
          wf(out, " = ");
          if (ins->operation.shift_type == O_ASHIFT) {
            switch (left_type->type) {
            case T_U8: wf(out, "(int8_t) "); break;
            case T_U16: wf(out, "(int16_t) "); break;
            case T_U32: wf(out, "(int32_t) "); break;
            case T_U64: wf(out, "(int64_t) "); break;
            default: break;
            }
          } else {
            switch (left_type->type) {
            case T_S8: wf(out, "(uint8_t) "); break;
            case T_S16: wf(out, "(uint16_t) "); break;
            case T_S32: wf(out, "(uint32_t) "); break;
            case T_S64: wf(out, "(uint64_t) "); break;
            default: break;
            }
          }
          wf(out, "instruction_");
          wi(out, ins->parameters[0]);
          wf(out, " >> instruction_")
          wi(out, ins->parameters[1]);
        } break;
        }
        break;
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
      wf(out, ";\n");
    }

    printf("block tail %zu\n", i);
    if (!block->is_final) {
      wf(out, "  ");
      switch (block->tail.type) {
      case GOTO: {
        wf(out, "instruction_");
        wi(out, block->tail.first_block);
        wc(out, '(');
        size_t params = block->tail.parameter_count;
        if (params) {
          wf(out, "instruction_");
          wi(out, block->tail.parameters[0]);

          for (size_t i = 1; i < params; i++) {
            wf(out, ", instruction_");
            wi(out, block->tail.parameters[i]);
          }
        }
        wc(out, ')');
      } break;
      case BRANCH: {
        wf(out, "(instruction_");
        wi(out, block->tail.condition);
        wf(out, " ? instruction_");
        wi(out, block->tail.first_block);
        wf(out, " : instruction_");
        wi(out, block->tail.second_block);
        wf(out, ")(");
        size_t params = block->tail.parameter_count;
        if (params) {
          wf(out, "instruction_");
          wi(out, block->tail.parameters[0]);

          for (size_t i = 1; i < params; i++) {
            wf(out, ", instruction_");
            wi(out, block->tail.parameters[i]);
          }
        }
        wc(out, ')');
      } break;
      }
      wf(out, ";\n");
    }

    wf(out, "}\n");
  }

  wf(out, "\n\n"
    "int main() {\n"
    "  block_0();\n"
    "  return 0;\n"
    "}\n");
}
