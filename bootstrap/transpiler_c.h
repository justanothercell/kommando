#ifndef TRANSPILER_C_H
#define TRANSPILER_C_H

#include <stdio.h>

#include "compiler.h"
#include "module.h"

void transpile_to_c(CompilerOptions options, FILE* header_stream, FILE* code_stream, str header_name, Program* program);

#endif