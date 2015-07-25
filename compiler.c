#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xalloc.h"
#include "expression.h"
#include "stream.h"
#include "parser.h"
#include "analyze.h"
#include "generator.h"
#include "backend.h"
#include "optimize.h"

static block_statement *parse_file(char *filename) {
  FILE *src;
  stream *in;
  block_statement *root;

  src = fopen(filename, "r");
  if (src == NULL) {
    fprintf(stderr, "compiler: no such file or directory\n");
    exit(1);
  }
  in = open_stream(src);
  root = parse(in);
  close_stream(in);
  fclose(src);

  return root;
}

static void backend_write_file(char *filename, code_system *system) {
  FILE *dest;

  dest = fopen(filename, "w");
  if (dest == NULL) {
    fprintf(stderr, "compiler: error opening output-file\n");
    exit(1);
  }

  backend_write(system, dest);

  fflush(dest);
  fclose(dest);
}

static block_statement *wrap_core(block_statement *root) {
  char *suffix = "/lib/core.cub";
  char *dir = dirname(__FILE__);
  size_t dirlen = strlen(dir);
  char *core = xrealloc(dir, dirlen + strlen(suffix) + sizeof(char));
  strcpy(core + dirlen, suffix);

  block_statement *core_block = parse_file(core);
  statement **tail = &core_block->body;
  for (; *tail; tail = &(*tail)->next);
  *tail = (statement*) root;

  return core_block;
}

int main(int argc, char *argv[]) {
  block_statement *root;
  code_system *system;

  if (argc < 3) {
    fprintf(stderr, "usage: compiler <input-file> <output-file>\n");
    return 1;
  }

  root = parse_file(argv[1]);
  root = wrap_core(root);

  analyze(root);
  system = generate(root);
  optimize(system);

  backend_write_file(argv[2], system);

  // http://stackoverflow.com/q/31622764/345645
  exit(0);
}
