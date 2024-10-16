#include "module.h"
#include "ast.h"
#include "lib.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/gc.h"
#include "lib/list.h"
#include "lib/map.h"
#include "parser.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

Identifier* gen_identifier(str name) {
    TokenStream* stream = tokenstream_new("<generated>", name);
    Identifier* ident = parse_identifier(stream);
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid identifier", name);
    return ident;
}

Path* gen_path(str path) {
    TokenStream* stream = tokenstream_new("<generated>", path);
    Path* p = parse_path(stream);
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid path", path);
    return p;
}

TypeValue* gen_typevalue(str typevalue, Span* span) {
    TokenStream* stream = tokenstream_new("<generated>", typevalue);
    TypeValue* tv = parse_type_value(stream);
    if (span != NULL) list_foreach(&tv->name->elements, lambda(void, (Identifier* ident) {
        ident->span = *span;
    }));
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid type value", typevalue);
    return tv;
}

TypeDef* gen_simple_type(str name) {
    Identifier* ident = gen_identifier(name);
    TypeDef* td = gc_malloc(sizeof(TypeDef));
    td->name = ident;
    td->generics = NULL;
    td->extern_ref = NULL;
    td->fields = map_new();
    return td;
}

void register_extern_type(Map* items, str name, str extern_ref) {
    TypeDef* ty = gen_simple_type(name);
    ty->extern_ref = extern_ref;
    ModuleItem* ty_mi = gc_malloc(sizeof(ModuleItem));
    ty_mi->item = ty;
    ty_mi->type = MIT_STRUCT;
    ty_mi->span= ty->name->span;
    map_put(items, name, ty_mi);
}

void register_intrinsic(Map* items, str prototype, str intrinsic, Map* var_bindings, Map* type_bindings) {
    TokenStream* s = tokenstream_new("<generated>", prototype);
    FuncDef* fd = parse_function_definition(s);
    fd->body->yield_last = true;
    Expression* expr = gc_malloc(sizeof(Expression));
    CIntrinsic* ci = gc_malloc(sizeof(CIntrinsic));
    ci->var_bindings = var_bindings;
    ci->type_bindings = type_bindings;
    ci->c_expr = intrinsic;
    ci->ret_ty = fd->return_type;
    expr->type = EXPR_C_INTRINSIC;
    expr->expr = ci;
    expr->span = fd->name->span;
    list_append(&fd->body->expressions, expr);
    ModuleItem* mi = gc_malloc(sizeof(ModuleItem));
    mi->span = fd->name->span;
    mi->item = fd;
    mi->type = MIT_FUNCTION;
    map_put(items, fd->name->name, mi);
}

Module* gen_std() {
    Module* module = gc_malloc(sizeof(Module));
    module->path = gen_path("::std");
    module->imports = list_new(PathList);
    module->items = map_new();
    module->resolved = false;
    module->in_resolution = false;

    register_extern_type(module->items, "bool", "bool");
    register_extern_type(module->items, "opaque_ptr", "void*");
    register_extern_type(module->items, "c_void", "void");
    register_extern_type(module->items, "c_const_str_ptr", "const char*");

    register_extern_type(module->items, "i8", "int8_t");
    register_extern_type(module->items, "i16", "int16_t");
    register_extern_type(module->items, "i32", "int32_t");
    register_extern_type(module->items, "i64", "int64_t");
    register_extern_type(module->items, "isize", "intptr_t");

    register_extern_type(module->items, "u8", "uint8_t");
    register_extern_type(module->items, "u16", "uint16_t");
    register_extern_type(module->items, "u32", "uint32_t");
    register_extern_type(module->items, "u64", "uint64_t");
    register_extern_type(module->items, "usize", "uintptr_t");

    TypeDef* ty_unit = gen_simple_type("unit");
    ModuleItem* ty_unit_mi = gc_malloc(sizeof(ModuleItem));
    ty_unit_mi->item = ty_unit;
    ty_unit_mi->type = MIT_STRUCT;
    ty_unit_mi->span = ty_unit->name->span;
    map_put(module->items, "unit", ty_unit_mi);

    {
        Map* var_bindings = map_new();
        Variable* var = gc_malloc(sizeof(Variable));
        var->name = gen_identifier("t");
        map_put(var_bindings, "t", var);
        Map* type_bindings = map_new();
        map_put(type_bindings, "V", gen_typevalue("V", NULL));
        register_intrinsic(module->items, "fn typecast<T, V>(t: T) -> V { } ", "(*(@V*)&$t)", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        map_put(type_bindings, "T", gen_typevalue("T", NULL));
        register_intrinsic(module->items, "fn sizeof<T>() -> ::std::usize {} ", "sizeof(@T)", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        map_put(type_bindings, "T", gen_typevalue("T", NULL));
        register_intrinsic(module->items, "fn c_typename<T>() -> ::std::c_const_str_ptr {} ", "\"@T\"", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        register_intrinsic(module->items, "fn intrinsic_argc() -> ::std::usize {} ", "__global__argc", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        register_intrinsic(module->items, "fn intrinsic_argv() -> ::std::opaque_ptr {} ", "__global__argv", var_bindings, type_bindings);
    }

    return module;
}