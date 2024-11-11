#ifndef MODULE_H
#define MODULE_H

#include "lib.h"
#include "ast.h"
#include "lib/list.h"

LIST(ImportList, Import*);

ENUM(ModuleItemType,
    MIT_FUNCTION,
    MIT_STRUCT,
    MIT_MODULE
);

typedef struct ModuleItem {
    void* item;
    ModuleItemType type;
} ModuleItem;

LIST(MIList, ModuleItem*);

typedef struct Module {
    Path* path;
    Map* items;
    PathList imports;
    bool resolved;
    bool in_resolution;
} Module;

typedef struct Program {
    Map* packages;
    Module* main_module;
} Program;

void insert_module(Program* program, Module* module);

Module* gen_intrinsics();
Module* gen_intrinsics_types();

Identifier* gen_identifier(str name);
TypeValue* gen_typevalue(str typevalue, Span* span);
Path* gen_path(str path);

#endif