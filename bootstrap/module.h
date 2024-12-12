#ifndef MODULE_H
#define MODULE_H

#include "lib.h"
#include "ast.h"

LIST(ImportList, Import*);

ENUM(ModuleItemType,
    MIT_FUNCTION,
    MIT_STRUCT,
    MIT_STATIC,
    MIT_MODULE,
    MIT_ANY
);

typedef struct ModuleItem {
    void* item;
    Identifier* name;
    ModuleItemType type;
    Visibility vis;
    Module* module;
    struct ModuleItem* origin;
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
    Module* parent;
    Visibility vis;
    ImplList impls;
    Map* package_method_map;
} Module;
typedef struct MethodImpl {
    TypeValue* tv;
    GenericKeys* keys;
    FuncDef* func;
} MethodImpl;
LIST(MethodImplList, MethodImpl);

typedef struct Program {
    Map* packages;
    Module* main_module;
} Program;

void insert_module(Program* program, Module* module, Visibility vis);

Module* gen_intrinsics();
Module* gen_intrinsics_types();

Identifier* gen_identifier(str name);
TypeValue* gen_typevalue(str typevalue, Span* span);
Path* gen_path(str path);
#endif