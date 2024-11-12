#include "module.h"
#include "ast.h"
#include "lib.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/list.h"
#include "lib/map.h"
#include "parser.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

void insert_module(Program* program, Module* module) {
    Path* path = module->path;
    if (path->elements.length == 1) {
        Identifier* name = path->elements.elements[0];
        if (map_put(program->packages, name->name, module) != NULL) spanned_error("Package already exists", name->span, "Cannot register package %s, it already exists.", to_str_writer(s, fprint_path(s, path)));
        return;
    }
    usize i = 0;
    Identifier* package = path->elements.elements[i];
    i += 1;
    if (!path->absolute) spanned_error("Module path should be absolute", package->span, "Module %s is not absolute", to_str_writer(s, fprint_path(s, path)));
    Module* mod = map_get(program->packages, package->name);
    if (mod == NULL) spanned_error("No such package", package->span, "Package %s does not exist. Try including it with the command line args: ::%s=path/to/package.kdo", package->name, package->name);
    Module* current = mod;
    while (i < path->elements.length - 1) {
        Identifier* m = path->elements.elements[i];
        i += 1;
        ModuleItem* it = map_get(current->items, m->name);
        if (it == NULL) spanned_error("No such module", m->span, "Module %s does not exist.", m->name);
        if (it->type != MIT_MODULE) spanned_error("Is not a module", m->span, "Item %s is of type %s, expected it to be a module", m->name, ModuleItemType__NAMES[it->type]);
        current = it->item;
    }
    ModuleItem* item = malloc(sizeof(ModuleItem));
    item->type = MIT_MODULE;
    item->item = module;
    Identifier* name = path->elements.elements[i];
    if (map_put(current->items, name->name, item) != NULL) spanned_error("Item already exists", name->span, "Cannot register module %s, item of such name already exists in parent module.", to_str_writer(s, fprint_path(s, path)));
}

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
    if (span != NULL) list_foreach(&tv->name->elements, lambda(void, Identifier* ident, {
        ident->span = *span;
    }));
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid type value", typevalue);
    return tv;
}

TypeDef* gen_simple_type(str name) {
    Identifier* ident = gen_identifier(name);
    TypeDef* td = malloc(sizeof(TypeDef));
    td->name = ident;
    td->generics = NULL;
    td->extern_ref = NULL;
    td->fields = map_new();
    td->transpile_state = 0;
    td->module = NULL;
    td->head_resolved = false;
    return td;
}

void register_extern_type(Module* module, str name, str extern_ref) {
    TypeDef* ty = gen_simple_type(name);
    ty->extern_ref = extern_ref;
    ModuleItem* ty_mi = malloc(sizeof(ModuleItem));
    ty_mi->item = ty;
    ty_mi->type = MIT_STRUCT;
    ty->module = module;
    map_put(module->items, name, ty_mi);
}

void register_intrinsic(Module* module, str prototype, str intrinsic, Map* var_bindings, Map* type_bindings) {
    TokenStream* s = tokenstream_new("<generated>", prototype);
    FuncDef* fd = parse_function_definition(s);
    fd->body->yield_last = true;
    Expression* expr = malloc(sizeof(Expression));
    CIntrinsic* ci = malloc(sizeof(CIntrinsic));
    ci->var_bindings = var_bindings;
    ci->type_bindings = type_bindings;
    ci->c_expr = intrinsic;
    ci->ret_ty = fd->return_type;
    expr->type = EXPR_C_INTRINSIC;
    expr->expr = ci;
    expr->span = fd->name->span;
    list_append(&fd->body->expressions, expr);
    ModuleItem* mi = malloc(sizeof(ModuleItem));
    mi->item = fd;
    mi->type = MIT_FUNCTION;
    fd->module = module;
    map_put(module->items, fd->name->name, mi);
}

Module* gen_intrinsics_types() {
    Module* module = malloc(sizeof(Module));
    module->path = gen_path("::intrinsics::types");
    module->imports = list_new(PathList);
    module->items = map_new();
    module->resolved = false;
    module->in_resolution = false;

    register_extern_type(module, "bool", "bool");
    register_extern_type(module, "opaque_ptr", "void*");
    register_extern_type(module, "c_void", "void");
    register_extern_type(module, "c_const_str_ptr", "const char*");

    register_extern_type(module, "i8", "int8_t");
    register_extern_type(module, "i16", "int16_t");
    register_extern_type(module, "i32", "int32_t");
    register_extern_type(module, "i64", "int64_t");
    register_extern_type(module, "isize", "intptr_t");

    register_extern_type(module, "u8", "uint8_t");
    register_extern_type(module, "u16", "uint16_t");
    register_extern_type(module, "u32", "uint32_t");
    register_extern_type(module, "u64", "uint64_t");
    register_extern_type(module, "usize", "uintptr_t");

    register_extern_type(module, "f32", "float");
    register_extern_type(module, "f64", "double");

    TypeDef* ty_unit = gen_simple_type("unit");
    ModuleItem* ty_unit_mi = malloc(sizeof(ModuleItem));
    ty_unit_mi->item = ty_unit;
    ty_unit_mi->type = MIT_STRUCT;
    ty_unit->module = module;
    map_put(module->items, "unit", ty_unit_mi);

    TypeDef* ty_ptr = gen_simple_type("ptr");
    TokenStream* ty_ptr_gs = tokenstream_new("<generated>", "<T> ");
    ty_ptr->generics = parse_generic_keys(ty_ptr_gs);
    ty_ptr->extern_ref = "void*";
    ModuleItem* ty_ptr_mi = malloc(sizeof(ModuleItem));
    ty_ptr_mi->item = ty_ptr;
    ty_ptr_mi->type = MIT_STRUCT;
    ty_ptr->module = module;
    map_put(module->items, "ptr", ty_ptr_mi);

    return module;
}

Module* gen_intrinsics() {
    Module* module = malloc(sizeof(Module));
    module->path = gen_path("::intrinsics");
    module->imports = list_new(PathList);
    module->items = map_new();
    module->resolved = false;
    module->in_resolution = false;

    {
        Map* var_bindings = map_new();
        Variable* var = malloc(sizeof(Variable));
        var->name = gen_identifier("t");
        map_put(var_bindings, "t", var);
        Map* type_bindings = map_new();
        map_put(type_bindings, "V", gen_typevalue("V", NULL));
        register_intrinsic(module, "fn typecast<T, V>(t: T) -> V { } ", "(*(@!V*)&$!t)", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Variable* var = malloc(sizeof(Variable));
        var->name = gen_identifier("t");
        map_put(var_bindings, "t", var);
        Map* type_bindings = map_new();
        map_put(type_bindings, "V", gen_typevalue("V", NULL));
        register_intrinsic(module, "fn numcast<T, V>(t: T) -> V { } ", "((@!V)$!t)", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        map_put(type_bindings, "T", gen_typevalue("T", NULL));
        register_intrinsic(module, "fn sizeof<T>() -> ::std::usize {} ", "sizeof(@!T)", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        map_put(type_bindings, "T", gen_typevalue("T", NULL));
        register_intrinsic(module, "fn c_typename<T>() -> ::std::c_const_str_ptr {} ", "\"@!T\"", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        map_put(type_bindings, "T", gen_typevalue("T", NULL));
        register_intrinsic(module, "fn typename<T>() -> ::std::c_const_str_ptr {} ", "\"@:T\"", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        map_put(type_bindings, "T", gen_typevalue("T", NULL));
        register_intrinsic(module, "fn short_typename<T>() -> ::std::c_const_str_ptr {} ", "\"@.T\"", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        register_intrinsic(module, "fn intrinsic_argc() -> ::std::usize {} ", "__global_argc", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        register_intrinsic(module, "fn intrinsic_argv() -> ::std::opaque_ptr {} ", "__global_argv", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        register_intrinsic(module, "fn stdin() -> ::std::opaque_ptr {} ", "stdin", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        register_intrinsic(module, "fn stdout() -> ::std::opaque_ptr {} ", "stdout", var_bindings, type_bindings);
    }

    {
        Map* var_bindings = map_new();
        Map* type_bindings = map_new();
        register_intrinsic(module, "fn stderr() -> ::std::opaque_ptr {} ", "stderr", var_bindings, type_bindings);
    }

    return module;
}