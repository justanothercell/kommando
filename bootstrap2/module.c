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
    td->generics = list_new(IdentList);
    td->extern_ref = NULL;
    return td;
}

void register_extern_type(Map* items, str name, str extern_ref) {
    TypeDef* ty_i32 = gen_simple_type(name);
    ty_i32->extern_ref = extern_ref;
    ModuleItem* ty_i32_mi = gc_malloc(sizeof(ModuleItem));
    ty_i32_mi->item = ty_i32;
    ty_i32_mi->type = MIT_STRUCT;
    map_put(items, name, ty_i32_mi);
}

Module* gen_std() {
    Module* module = gc_malloc(sizeof(Module));
    module->path = gen_path("::std");
    module->imports = list_new(PathList);
    module->items = map_new();
    module->resolved = false;

    register_extern_type(module->items, "bool", "bool");
    register_extern_type(module->items, "opaque_ptr", "void*");
    register_extern_type(module->items, "c_const_str_ptr", "const char*");

    register_extern_type(module->items, "i8", "int8_t");
    register_extern_type(module->items, "i16", "int16_t");
    register_extern_type(module->items, "i32", "int32_t");
    register_extern_type(module->items, "i64", "int64_t");

    register_extern_type(module->items, "u8", "uint8_t");
    register_extern_type(module->items, "u16", "uint16_t");
    register_extern_type(module->items, "u32", "uint32_t");
    register_extern_type(module->items, "u64", "uint64_t");

    TypeDef* ty_unit = gen_simple_type("unit");
    ModuleItem* ty_unit_mi = gc_malloc(sizeof(ModuleItem));
    ty_unit_mi->item = ty_unit;
    ty_unit_mi->type = MIT_STRUCT;
    map_put(module->items, "unit", ty_unit_mi);

    return module;
}