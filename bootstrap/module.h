#ifndef MODULE_H
#define MODULE_H

#include "lib.h"
#include "ast.h"

LIST(ImportList, Import*);

typedef struct Module {
    Path* path;
    Map* items;
    PathList imports;
    bool resolved;
    bool in_resolution;
} Module;

typedef struct Program {
    Map* modules;
    Module* main_module;
} Program;

Module* gen_intrinsics();

Identifier* gen_identifier(str name);
TypeValue* gen_typevalue(str typevalue, Span* span);
Path* gen_path(str path);

#endif