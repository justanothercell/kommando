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
    module->functions = map_new();
    module->types = map_new();
    module->resolved = false;

    FuncDef* puts = gc_malloc(sizeof(FuncDef));
    puts->is_variadic = false;
    puts->name = gen_identifier("puts");
    puts->args = list_new(ArgumentList);
    puts->body = gc_malloc(sizeof(Block));
    puts->body->expressions = list_new(ExpressionList);
    Argument* arg = gc_malloc(sizeof(Argument));
    Variable* var = gc_malloc(sizeof(Variable));
    var->id = 0;
    var->name = gen_identifier("s");
    arg->var = var;
    arg->type = gen_typevalue("::std::Ptr<::std::u8> ");
    arg->type->def = NULL;
    list_append(&puts->args, arg);
    puts->return_type = gen_typevalue("::std::i32");
    map_put(module->functions, "puts", puts);

    TypeDef* ty_i32 = gen_simple_type("i32"); 
    map_put(module->types, "i32", ty_i32);
    TypeDef* ty_bool = gen_simple_type("bool"); 
    map_put(module->types, "bool", ty_bool);
    TypeDef* ty_u8 = gen_simple_type("u8");
    map_put(module->types, "u8", ty_u8);
    TypeDef* ty_unit = gen_simple_type("unit");
    map_put(module->types, "unit", ty_unit);
    TypeDef* ty_ptr = gen_simple_type("Ptr");
    list_append(&ty_ptr->generics, gen_identifier("T"));
    map_put(module->types, "Ptr", ty_ptr);

    return module;
}