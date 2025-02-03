#ifndef MODULE_H
#define MODULE_H

#include "lib.h"
#include "ast.h"

LIST(ImportList, Import*);

ENUM(ModuleItemType,
    MIT_FUNCTION,
    MIT_STRUCT,
    MIT_STATIC,
    MIT_CONSTANT,
    MIT_TRAIT,
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
LIST(FuncList, FuncDef*);

typedef struct TraceGen {
    Variable* top_frame;
    str top_frame_c_name;
    TypeValue* frame_type;
    str frame_type_c_name;
    TypeValue* function_type;
    str function_type_c_name;
    str local_frame_name;
    bool trace_this;
} TraceGen;

typedef struct Program {
    Map* packages;
    Module* main_module;
    TraceGen tracegen;
} Program;

typedef struct CompilerOptions CompilerOptions;
void insert_module(Program* program, CompilerOptions* options, Module* module, Visibility vis);

Module* gen_core_intrinsics();
Module* gen_core_types();

Identifier* gen_identifier(str name);
TypeValue* gen_typevalue(str typevalue, Span* span);
Path* gen_path(str path);
#endif