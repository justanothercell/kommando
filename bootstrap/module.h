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
    Map* package_trait_impl_map;
} Module;
LIST(FuncList, FuncDef*);

typedef struct TraceGen {
    Variable* top_frame;
    str top_frame_c_name;
    TypeValue* frame_type;
    str frame_type_c_name;
    TypeValue* function_type;
    str function_type_c_name;
    bool trace_this;
} TraceGen;

typedef struct RaiiData {
    str copy_key;
    TraitDef* copy;
    str clone_key;
    TraitDef* clone;
    str drop_key;
    TraitDef* drop;
    TypeDef* raw;
} RaiiData;

typedef struct QueuedFunc {
    str key;
    FuncDef* func;
    GenericValues* type_generics;
    GenericValues* func_generics;
} QueuedFunc;
LIST(QFList, QueuedFunc);

typedef struct QueuedType {
    str key;
    TypeValue* type;
} QueuedType;
LIST(QTList, QueuedType);

typedef struct TranspilerData {
    Map* type_map;
    Map* type_done_map;
    Map* func_map;
    QTList type_queue;
    QFList func_queue;
} TranspilerData;

typedef struct Program {
    Map* packages;
    Module* main_module;
    TraceGen tracegen;
    RaiiData raii;
    TranspilerData t;
} Program;

typedef struct CompilerOptions CompilerOptions;
void insert_module(Program* program, CompilerOptions* options, Module* module, Visibility vis);

Identifier* gen_identifier(str name);
TypeValue* gen_typevalue(str typevalue, Span* span);
Path* gen_path(str path, Span* span);
#endif