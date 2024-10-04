#ifndef MODULE_H
#define MODULE_H

#include "lib.h"
#include "ast.h"

typedef struct Module {
    Path* path;
    Map* functions;
    Map* types;
    PathList imports;
    bool resolved;
} Module;

typedef struct Program {
    Map* modules;
    Module* main_module;
} Program;

Module* gen_std();

Identifier* gen_identifier(str name);
Path* gen_path(str path);

#endif