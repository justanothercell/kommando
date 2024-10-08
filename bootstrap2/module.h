#ifndef MODULE_H
#define MODULE_H

#include "lib.h"
#include "ast.h"

typedef struct Module {
    Path* path;
    Map* items;
    PathList imports;
    bool resolved;
} Module;

typedef struct Program {
    Map* modules;
    Module* main_module;
} Program;

Module* gen_std();

Identifier* gen_identifier(str name);
TypeValue* gen_typevalue(str typevalue);
Path* gen_path(str path);

#endif