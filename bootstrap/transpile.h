#ifndef TRANSPILE_H
#define TRANSPILE_H

#include "lib/defines.h"

#include "ast.h"

#include <stdio.h>

void write_code(FILE* dest, const str format, ...);

void write_indent(FILE* dest, usize indent);

typedef struct Writer {
    FILE* code;
    FILE* header;
    bool gen_main;
} Writer;

Writer* new_writer(str name, FILE* code, FILE* header, bool gen_main);

void drop_writer(Writer* writer);

void finalize_transpile(Writer* writer);

void transpile_expression(Module* module, Writer* writer, Expression* expr, GenericsMapping* mapping, usize indent);

void transpile_block(Module* module, Writer *writer, Block* block, GenericsMapping* mapping, usize indent);

void transpile_function(Writer* writer, Module* module, FunctionDef* func, GenericsMapping* mapping);

void transpile_type_def(Writer* writer, Module* module, TypeDef* tydef);

void transpile_module(Writer* writer, Module* module);

#endif