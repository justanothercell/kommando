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

TypeValue* gen_typevalue(str typevalue) {
    TokenStream* stream = tokenstream_new("<generated>", typevalue);
    TypeValue* tv = parse_type_value(stream);
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid type value", typevalue);
    return tv;
}

TypeDef* gen_simple_type(str name) {
    Identifier* ident = gen_identifier(name);
    TypeDef* td = gc_malloc(sizeof(TypeDef));
    td->name = ident;
    td->generics = list_new(IdentList);
    return td;
}

Module* gen_std() {
    Module* module = gc_malloc(sizeof(Module));
    module->path = gen_path("::std");
    module->imports = list_new(PathList);
    module->items = map_new();
    module->resolved = false;

    FuncDef* puts = gc_malloc(sizeof(FuncDef));
    puts->is_variadic = false;
    puts->name = gen_identifier("puts");
    puts->args = list_new(ArgumentList);
    puts->body = NULL;
    Argument* arg = gc_malloc(sizeof(Argument));
    Variable* var = gc_malloc(sizeof(Variable));
    var->id = 0;
    var->name = gen_identifier("s");
    arg->var = var;
    arg->type = gen_typevalue("::std::Ptr<::std::u8> ");
    list_append(&puts->args, arg);
    puts->return_type = gen_typevalue("::std::i32");
    puts->generic_uses = map_new();
    ModuleItem* puts_mi = gc_malloc(sizeof(ModuleItem));
    puts_mi->item = puts;
    puts_mi->type = MIT_FUNCTION;
    map_put(module->items, "puts", puts_mi);

    TypeDef* ty_i32 = gen_simple_type("i32");
    ty_i32->extern_ref = "int32_t";
    ModuleItem* ty_i32_mi = gc_malloc(sizeof(ModuleItem));
    ty_i32_mi->item = ty_i32;
    ty_i32_mi->type = MIT_STRUCT;
    map_put(module->items, "i32", ty_i32_mi);
    TypeDef* ty_bool = gen_simple_type("bool");
    ty_bool->extern_ref = "bool";
    ModuleItem* ty_bool_mi = gc_malloc(sizeof(ModuleItem));
    ty_bool_mi->item = ty_bool;
    ty_bool_mi->type = MIT_STRUCT;
    map_put(module->items, "bool", ty_bool_mi);
    TypeDef* ty_u8 = gen_simple_type("u8");
    ty_u8->extern_ref = "uint8_t";
    ModuleItem* ty_u8_mi = gc_malloc(sizeof(ModuleItem));
    ty_u8_mi->item = ty_u8;
    ty_u8_mi->type = MIT_STRUCT;
    map_put(module->items, "u8", ty_u8_mi);
    TypeDef* ty_unit = gen_simple_type("unit");
    ModuleItem* ty_unit_mi = gc_malloc(sizeof(ModuleItem));
    ty_unit_mi->item = ty_unit;
    ty_unit_mi->type = MIT_STRUCT;
    map_put(module->items, "unit", ty_unit_mi);
    TypeDef* ty_ptr = gen_simple_type("Ptr");
    ty_ptr->extern_ref = "void*";
    list_append(&ty_ptr->generics, gen_identifier("T"));
    ModuleItem* ty_ptr_mi = gc_malloc(sizeof(ModuleItem));
    ty_ptr_mi->item = ty_ptr_mi;
    ty_ptr_mi->type = MIT_STRUCT;
    map_put(module->items, "Ptr", ty_ptr_mi);

    return module;
}