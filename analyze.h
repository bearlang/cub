#ifndef ANALYZE_H
#define ANALYZE_H

#include "statement.h"

#define ST_CLASS 1
#define ST_TYPE 2
#define ST_VARIABLE 4

void analyze(block_statement*);

#endif
