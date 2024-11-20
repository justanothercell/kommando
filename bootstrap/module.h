#ifndef MODULE_H
#define MODULE_H

#include "lib.h"
#include "ast.h"
#include "lib/list.h"

LIST(ImportList, Import*);

ENUM(ModuleItemType,
    MIT_FUNCTION,
    MIT_STRUCT,
    MIT_MODULE,
    MIT_ANY
);

typedef struct ModuleItem {
    void* item;
    ModuleItemType type;
    bool pub;
    Module* module;
} ModuleItem;

LIST(MIList, ModuleItem*);

typedef struct Module {
    Identifier* name;
    Path* path;
    Map* items;
    ImportList imports;
    bool resolved;
    bool in_resolution;
    ModDefList subs;
    str filepath;
} Module;

typedef struct Program {
    Map* packages;
    Module* main_module;
} Program;

void insert_module(Program* program, Module* module, bool pub);

Module* gen_intrinsics();
Module* gen_intrinsics_types();

Identifier* gen_identifier(str name);
TypeValue* gen_typevalue(str typevalue, Span* span);
Path* gen_path(str path);
#endif