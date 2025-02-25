#ifndef TRANSPILER_C_H
#define TRANSPILER_C_H

#include <stdio.h>

#include "compiler.h"
#include "module.h"

void transpile_to_c(Program* program, CompilerOptions* options, FILE* type_h_stream, FILE* object_h_stream, FILE* code_stream, str base_name);

#endif