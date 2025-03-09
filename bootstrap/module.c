#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "module.h"
#include "ast.h"
#include "compiler.h"
#include "lib.h"
#include "lib/list.h"
#include "lib/map.h"
LIB;
#include "parser.h"
#include "token.h"

void insert_module(Program* program, CompilerOptions* options, Module* module, Visibility vis) {
    module->vis = vis;
    module->parent = NULL;
    Path* path = module->path;
    if (path->elements.length == 1) {
        Identifier* name = path->elements.elements[0];
        module->package_method_map = map_new();
        module->package_trait_impl_map = map_new();
        if (vis != V_PUBLIC) spanned_error("Package must be public", name->span, "Cannot register package %s as %s, this is probably a compiler error.", to_str_writer(s, fprint_path(s, path)), Visibility__NAMES[vis]);
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
    item->module = current;
    item->origin = NULL;
    item->vis = vis;
    item->name = module->name;
    Identifier* name = path->elements.elements[i];
    
    module->parent = current;
    Module* pkg = current;
    while (pkg->parent != NULL) pkg = pkg->parent;
    module->package_method_map = pkg->package_method_map;
    module->package_trait_impl_map = pkg->package_trait_impl_map;

    ModuleItem* old = map_put(current->items, name->name, item);
    if (old != NULL) {
        spanned_error("Name conflict", name->span, "Name %s is already defined in this scope at %s", name->name, to_str_writer(s, fprint_span(s, &old->name->span)));
    }

    if (options->verbosity >= 3) log("Registered module %s", to_str_writer(s, fprint_path(s, module->path)));
}

Identifier* gen_identifier(str name) {
    TokenStream* stream = tokenstream_new("<generated>", name);
    Identifier* ident = parse_identifier(stream);
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid identifier", name);
    return ident;
}

Path* gen_path(str path, Span* span) {
    TokenStream* stream = tokenstream_new("<generated>", path);
    Path* p = parse_path(stream);
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid path", path);
    if (span != NULL) {
        for (usize i = 0;i < p->elements.length;i++) {
            p->elements.elements[i]->span = *span;
        }
    }
    return p;
}

static void apply_span(TypeValue* tv, Span span) {
    list_foreach(&tv->name->elements, i, Identifier* ident, {
        ident->span = span;
    });
    if (tv->generics != NULL) {
        tv->generics->span = span;
        list_foreach(&tv->generics->generics, counter, TypeValue* g, apply_span(g, span));
    }
}

TypeValue* gen_typevalue(str typevalue, Span* span) {
    TokenStream* stream = tokenstream_new("<generated>", typevalue);
    TypeValue* tv = parse_type_value(stream);
    if (span != NULL) apply_span(tv, *span);
    if(try_next_token(stream) != NULL) panic("`%s` is not a valid type value", typevalue);
    return tv;
}

TypeDef* gen_simple_type(str name) {
    Identifier* ident = gen_identifier(name);
    TypeDef* td = malloc(sizeof(TypeDef));
    td->key = NULL;
    td->name = ident;
    td->generics = NULL;
    td->extern_ref = NULL;
    td->flist = list_new(IdentList);
    td->fields = map_new();
    td->transpile_state = 0;
    td->module = NULL;
    td->traits = list_new(TraitBoundList);
    return td;
}

void register_extern_type(Module* module, str name, str extern_ref) {
    TypeDef* ty = gen_simple_type(name);
    ty->extern_ref = extern_ref;
    ty->module = module;
    ModuleItem* ty_mi = malloc(sizeof(ModuleItem));
    ty_mi->item = ty;
    ty_mi->type = MIT_STRUCT;
    ty_mi->vis = V_PUBLIC;
    ty_mi->module = module;
    ty_mi->origin = NULL;
    ty_mi->name = ty->name;
    ty->module = module;
    map_put(module->items, name, ty_mi);
}