#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lib.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
LIB
#include "resolver.h"
#include "ast.h"
#include "module.h"
#include "parser.h"
#include "resolver.h"
#include "token.h"

void fprint_res_tv(FILE* stream, TypeValue* tv) {
    if (tv->def == NULL) panic("TypeValue was not resolved %s @ %s", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &tv->name->elements.elements[0]->span)));
    if (tv->def->extern_ref != NULL) {
        fprintf(stream, "%s", tv->def->extern_ref);
        return;
    }
    fprintf(stream, "%s%p", tv->def->name->name, tv->def);
    if (tv->generics != NULL && tv->generics->generics.length > 0) {
        fprintf(stream, "<");
        list_foreach(&tv->generics->generics, i, TypeValue* t, {
            if (i > 0) fprintf(stream, ",");
            fprint_res_tv(stream, t);
        });
        fprintf(stream, ">");
    }
}

static str __gvals_to_key(GenericValues* generics, bool assert_resolved) {
    return to_str_writer(stream, {
        if (generics != NULL && generics->generics.length > 0) {
            fprintf(stream, "<");
            list_foreach(&generics->generics, i, TypeValue* tv, {
                if (i > 0) fprintf(stream, ",");
                if (tv->def == NULL) spanned_error("Unresolved typevalue", tv->name->elements.elements[0]->span, "%s is not resolved", to_str_writer(s, fprint_typevalue(s, tv)));
                if (tv->def->module != NULL) {
                    fprint_path(stream, tv->def->module->path);
                    fprintf(stream, "::");
                } else if (assert_resolved) spanned_error("Unexpanded generic", tv->name->elements.elements[0]->span, "Generic %s @ %s was not properly resolved", tv->def->name->name, to_str_writer(s, fprint_span(s, &tv->def->name->span))); 
                fprintf(stream, "%s", tv->def->name->name);
                
                fprintf(stream, "%s", __gvals_to_key(tv->generics, assert_resolved));
                if (tv->ctx != NULL) {
                    if (assert_resolved) spanned_error("Unresolved type", tv->name->elements.elements[0]->span, "Type %s @ %s is still in context. This is probably a compiler error.", tv->def->name->name, to_str_writer(s, fprint_span(s, &tv->def->name->span)));
                    fprintf(stream, "@%p<%s>", tv->ctx, to_str_writer(stream, {
                        list_foreach(&tv->ctx->generics, j, Identifier* g, {
                            if (j > 0) fprintf(stream, ",");
                            str key = g->name;
                            TypeDef* td = map_get(tv->ctx->resolved, key);
                            fprintf(stream, "%s%p", td->name->name, td);
                        });
                    }));
                }
            });
            fprintf(stream, ">");
        }
    });
}

str gvals_to_key(GenericValues* generics) {
    return __gvals_to_key(generics, false);
}

str gvals_to_c_key(GenericValues* generics) {
    return __gvals_to_key(generics, true);
}

str tfvals_to_key(GenericValues* type_generics, GenericValues* func_generics) {
    return to_str_writer(s, {
       fprintf(s, "%s", gvals_to_key(type_generics)); 
       fprintf(s, "%s", gvals_to_key(func_generics)); 
    });
}

void resovle_imports(Program* program, Module* module, List* mask);
static ModuleItem* resolve_item_raw(Program* program, Module* module, Path* path, ModuleItemType kind, List* during_imports_mask) {
    Module* context_module = module;
    Identifier* name = path->elements.elements[path->elements.length-1];
    ModuleItem* result;
    usize i = 0;
    Identifier* m = path->elements.elements[0];
    ModuleItem* mod = NULL;
    if (!path->absolute) mod = map_get(module->items, m->name);
    if (mod != NULL) { // try local
        if (mod->type == MIT_MODULE && during_imports_mask != NULL) resovle_imports(program, mod->item, during_imports_mask);
        if (path->elements.length == 1) {
            if (kind != MIT_ANY) if (mod->type != kind) spanned_error("Itemtype mismatch", name->span, "Item %s is of type %s, expected it to be %s", name->name, ModuleItemType__NAMES[mod->type], ModuleItemType__NAMES[kind]);
            result = mod;
            goto check_vis;
        }
        if (mod->type != MIT_MODULE) spanned_error("Is not a module", m->span, "Item %s is of type %s, expected it to be a module", m->name, ModuleItemType__NAMES[mod->type]);
        module = mod->item;
        i += 1;
    } else { // try global
        Module* package = map_get(program->packages, m->name);
        if (package != NULL) {
            if (during_imports_mask != NULL) resovle_imports(program, package, during_imports_mask);
            if (path->elements.length == 1) {
                if (MIT_MODULE != kind) spanned_error("Itemtype mismatch", name->span, "Item %s is of type %s, expected it to be %s", name->name, ModuleItemType__NAMES[MIT_MODULE], ModuleItemType__NAMES[kind]);
                mod = malloc(sizeof(ModuleItem));
                mod->item = package;
                mod->type = MIT_MODULE;
                mod->vis = V_PUBLIC; // all packages are public,otherwise they'd be useless
                mod->module = NULL;
                result = mod;
                goto check_vis;
            }
            i += 1;
            module = package;
        } else {
            spanned_error("No such item", m->span, "Item %s does not exist", m->name);
        }
    }
    while (i < path->elements.length - 1) {
        Identifier* m = path->elements.elements[i];
        i += 1;
        ModuleItem* it = map_get(module->items, m->name);
        if (it == NULL) spanned_error("No such module", m->span, "Module %s does not exist.", m->name);
        if (it->type != MIT_MODULE) spanned_error("Is not a module", m->span, "Item %s is of type %s, expected it to be a module", m->name, ModuleItemType__NAMES[it->type]);
        module = it->item;
        if (during_imports_mask != NULL) resovle_imports(program, module, during_imports_mask); 
    }
    ModuleItem* it = map_get(module->items, name->name);
    if (it == NULL) spanned_error("No such item", name->span, "%s %s does not exist.", ModuleItemType__NAMES[kind], name->name);
    if (kind != MIT_ANY) if (it->type != kind) spanned_error("Itemtype mismatch", name->span, "Item %s is of type %s, expected it to be %s", name->name, ModuleItemType__NAMES[it->type], ModuleItemType__NAMES[kind]);
    result = it;
    
    check_vis: {
        Module* mod_root = context_module;
        Module* result_root = result->module;
        if (result_root == NULL) return result; // skip checking, root modules are always public
        while (mod_root->parent != NULL) mod_root = mod_root->parent;
        while (result_root->parent != NULL) result_root = result_root->parent;
        Visibility required = V_PRIVATE;
        if (context_module != result->module) required = V_LOCAL;
        if (mod_root != result_root) required = V_PUBLIC;
        if (result->vis < required) spanned_error("Lacking visibility", path->elements.elements[0]->span, "Cannot get item %s::%s while in %s as this would require it having a visibility of %s,\nbut %s was only declared as %s",
                                            to_str_writer(s, fprint_path(s, result->module->path)), result->name->name, to_str_writer(s, fprint_path(s, context_module->path)), Visibility__NAMES[required], result->name->name, Visibility__NAMES[result->vis]);
        Module* res_tree = result->module;
        while (res_tree != NULL) {
            Module* ctx_tree = context_module;
            while (ctx_tree != NULL) { // wasteful but shush
                if (ctx_tree == res_tree) goto done_checking_tree;
                ctx_tree = ctx_tree->parent;
            }
            if (res_tree->vis < required) spanned_error("Lacking visibility", path->elements.elements[0]->span, "Cannot get item %s::%s while in %s as this would require the ancestor module %s to have a visibility of %s,\nbut %s was only declared as %s",
                                            to_str_writer(s, fprint_path(s, result->module->path)), result->name->name, to_str_writer(s, fprint_span(s, &result->module->path->elements.elements[0]->span)), Visibility__NAMES[required], to_str_writer(s, fprint_path(s, context_module->path)), to_str_writer(s, fprint_path(s, res_tree->path)), ctx_tree->name->name, to_str_writer(s, fprint_span(s, &result->name->span)), Visibility__NAMES[res_tree->vis]);
            res_tree = res_tree->parent;
        }
        done_checking_tree: {}
    }
    
    return result;
}

// this key is at some point instanciated with these values
// for example: 
//             Foo  <i32>              <T> (of Foo)            
//    register_item(tv->generics, NULL, tv->def->generics);
void register_item(GenericValues* type_values, GenericValues* func_values, GenericKeys* gkeys) {
    // no need to register if we are not generic - we just complie the one default nongeneric variant in that case
    if (type_values != NULL || func_values != NULL) {
        if (type_values == NULL && func_values != NULL) {
            type_values = func_values;
            func_values = NULL;
        }
        str key = tfvals_to_key(type_values, func_values);
        if (!map_contains(gkeys->generic_uses, key)) {
            GenericUse* use = malloc(sizeof(GenericUse));
            use->type_context = type_values;
            use->func_context = func_values;
            map_put(gkeys->generic_uses, key, use);
            list_append(&gkeys->generic_use_keys, key);
        }
    }
    if (type_values != NULL) {
        list_foreach(&type_values->generics, i, TypeValue* v, {
           register_item(v->generics, NULL, v->def->generics); 
        });
    }

    if (func_values != NULL) {
        list_foreach(&func_values->generics, i, TypeValue* v, {
           register_item(v->generics, NULL, v->def->generics); 
        });
    }
}

void resolve_funcdef(Program* program, FuncDef* func, GenericKeys* type_generics);
void resolve_typedef(Program* program, TypeDef* ty);
void resolve_typevalue(Program* program, Module* module, TypeValue* tval, GenericKeys* func_generics, GenericKeys* type_generics);
static int item_cache_hits = 0;
static int item_cache_misses = 0;
void report_item_cache_stats() {
    log("Item cache hits: %d misses: %d", item_cache_hits, item_cache_misses);
}
void* resolve_item(Program* program, Module* module, Path* path, ModuleItemType kind, GenericKeys* func_generics, GenericKeys* type_generics, GenericValues** gvalsref) {
    typedef struct {
        str key;
        ModuleItem* item;
    } MICacheItem;
    LIST(MICList, MICacheItem);
    static MICList LOOKUP_CACHE = list_new(MICList);
    const usize CACHE_SIZE = 16;
    /*
        // examples/generics.kdo
        // note that this is still a rather small example so it only is partially representative.
        size: hits/misses
            1:   40/295
            2:   71/264
            3:   90/245
            4:   95/240
            6:  109/226
            8:  112/223
           12:  145/190
           16:  161/174
           24:  193/142
           32:  197/138
           48:  214/121
           64:  217/118
           96:  246/ 89
          128:  246/ 89
          192:  246/ 89
          256:  246/ 89
    */
    str key = to_str_writer(s, {
        fprint_path(s, path);
        if (!path->absolute) {
            fprintf(s, "|");
            fprint_path(s, module->path);
        }
    });

    ModuleItem* mi = NULL;
    for (usize i = 0;i < CACHE_SIZE && i < LOOKUP_CACHE.length;i++) {
        if (str_eq(LOOKUP_CACHE.elements[i].key, key)) {
            mi = LOOKUP_CACHE.elements[i].item;
            if (i > 0) { // improve location in cache
                MICacheItem t = LOOKUP_CACHE.elements[i];
                LOOKUP_CACHE.elements[i] = LOOKUP_CACHE.elements[i - 1];
                LOOKUP_CACHE.elements[i - 1] = t;
            }
            item_cache_hits += 1;
            break;
        }
    }

    if (mi == NULL) {
        mi = resolve_item_raw(program, module, path, kind, NULL);
        MICacheItem mic = (MICacheItem) { .key=key, .item = mi };
        if (LOOKUP_CACHE.length < CACHE_SIZE) list_append(&LOOKUP_CACHE, mic);
        else LOOKUP_CACHE.elements[CACHE_SIZE - 1] = mic;
        item_cache_misses += 1;
    }
#ifdef DEBUG_CACHE
    {
        FILE* cachelog = NULL;
        if (item_cache_hits + item_cache_misses == 1) {
            cachelog = fopen("CACHELOG.txt", "w");
        } else {
            cachelog = fopen("CACHELOG.txt", "a");
        }
        list_foreach_i(&LOOKUP_CACHE, lambda(void, usize i, MICacheItem item, {
            if (i > 0) fprintf(cachelog, ", ");
            fprintf(cachelog, "%s", item.key);
        }));
        fprintf(cachelog, "\n");
        fclose(cachelog);
    }
#endif

    switch (mi->type) {
        case MIT_FUNCTION: {
            FuncDef* func = mi->item;
            if (!func->head_resolved) resolve_funcdef(program, func, NULL);
        } break;
        case MIT_STRUCT: {
            TypeDef* type = mi->item;
            if (!type->head_resolved) resolve_typedef(program, type);
        } break;
        case MIT_STATIC: {
            Static* s = mi->item;
            if (s->type->def == NULL) resolve_typevalue(program, module, s->type, func_generics, type_generics);
        } break;
        default:
            unreachable();
    }
    GenericKeys* gkeys = NULL;
    switch (mi->type) {
        case MIT_FUNCTION:
            gkeys = ((FuncDef*)mi->item)->generics;
            break;
        case MIT_STRUCT:
            gkeys = ((TypeDef*)mi->item)->generics;
            break;
        case MIT_STATIC:
            break;
        default:
            unreachable();
    }
    str fullname = to_str_writer(stream, fprint_path(stream, path));
    GenericValues* gvals = *gvalsref;
    Span span = from_points(&path->elements.elements[0]->span.left, &path->elements.elements[path->elements.length-1]->span.right);
    if ((gkeys == NULL || gkeys->generics.length == 0) && (gvals == NULL || gvals->generics.length == 0)) goto generic_end;
    if (gkeys == NULL && gvals != NULL) spanned_error("Unexpected generics", span, "Expected no generics for %s, got %llu", fullname, gvals->generics.length);
    if (gkeys != NULL && gvals == NULL) {
        gvals = malloc(sizeof(GenericValues));
        gvals->span = span;
        gvals->generics = list_new(TypeValueList);
        gvals->resolved = map_new();
        list_foreach(&gkeys->generics, i, Identifier* generic, {
            TypeValue* dummy = gen_typevalue("_", &span);
            list_append(&gvals->generics, dummy);
            map_put(gvals->resolved, generic->name, dummy);
        });
        *gvalsref = gvals;
    }
    if (gkeys->generics.length != gvals->generics.length) spanned_error("Generics mismatch", gvals->span, "Expected %llu for %s, got %llu", gkeys->generics.length, fullname, gvals->generics.length);
    // We definitely have some generics here!
    for (usize i = 0;i < gvals->generics.length;i++) {
        str generic_key = gkeys->generics.elements[i]->name;
        TypeValue* generic_value = gvals->generics.elements[i];
        resolve_typevalue(program, module, generic_value, func_generics, type_generics);
        map_put(gvals->resolved, generic_key, generic_value);
        if (generic_value->def == NULL) continue;
        if (func_generics != NULL && map_contains(func_generics->resolved, generic_value->def->name->name)) {
            if (generic_value->ctx != NULL && generic_value->ctx != func_generics) spanned_error("Multiple contexts found", generic_value->name->elements.elements[0]->span, "%s has potential contexts in %s @ %s and %s @ %s",
                                                generic_value->def->name->name,
                                                to_str_writer(s, fprint_span_contents(s, &generic_value->ctx->span)), to_str_writer(s, fprint_span(s, &generic_value->ctx->span)),
                                                to_str_writer(s, fprint_span_contents(s, &func_generics->span)), to_str_writer(s, fprint_span(s, &func_generics->span))
                                            );
            generic_value->ctx = func_generics;
        }
        if (type_generics != NULL && map_contains(type_generics->resolved, generic_value->def->name->name)) {
            if (generic_value->ctx != NULL && generic_value->ctx != func_generics) spanned_error("Multiple contexts found", generic_value->name->elements.elements[0]->span, "%s has potential contexts in %s @ %s and %s @ %s",
                                                generic_value->def->name->name,
                                                to_str_writer(s, fprint_span_contents(s, &generic_value->ctx->span)), to_str_writer(s, fprint_span(s, &generic_value->ctx->span)),
                                                to_str_writer(s, fprint_span_contents(s, &type_generics->span)), to_str_writer(s, fprint_span(s, &type_generics->span))
                                            );
            generic_value->ctx = func_generics; // don't question it... we are still dependant on the calling function even if the generic comes from the implementing type on a method
        }
    }
    generic_end: {}
    if (kind == MIT_ANY) return mi;
    return mi->item;
}

void resolve_typevalue(Program* program, Module* module, TypeValue* tval, GenericKeys* func_generics, GenericKeys* type_generics) {
    if (tval->def != NULL) return;
    // might be a generic? >`OvO´<
    if (!tval->name->absolute && tval->name->elements.length == 1) {
        str key = tval->name->elements.elements[0]->name;
        if (str_eq(key, "_")) {
            return;
        }
        TypeDef* func_gen_t = NULL;
        TypeDef* type_gen_t = NULL;
        if (func_generics != NULL) {
            func_gen_t = map_get(func_generics->resolved, key);
        }
        if (type_generics != NULL) {
            type_gen_t = map_get(type_generics->resolved, key);
        }
        if (func_gen_t != NULL && type_gen_t != NULL) spanned_error("Multiple generic candidates", tval->name->elements.elements[0]->span, "Generic %s could belong to %s or %s", 
                    tval->name->elements.elements[0]->name,
                    to_str_writer(stream, fprint_span(stream, &func_gen_t->name->span)),
                    to_str_writer(stream, fprint_span(stream, &type_gen_t->name->span)));
        if ((func_gen_t != NULL || type_gen_t != NULL) && tval->generics != NULL) spanned_error("Generic generic", tval->generics->span, "Generic parameter should not have generic arguments @ %s", to_str_writer(s, fprint_span(s, &tval->def->name->span)));
        if (func_gen_t != NULL) {
            tval->def = func_gen_t;
            return;
        }
        if (type_gen_t != NULL) {
            tval->def = type_gen_t;
            return;
        }
    }
    // here we are no longer generic
    TypeDef* td = resolve_item(program, module, tval->name, MIT_STRUCT, func_generics, type_generics, &tval->generics);
    tval->def = td;
    if (tval->generics != NULL) {
        list_foreach(&tval->generics->generics, i, TypeValue* tv, {
            resolve_typevalue(program, module, tv, func_generics, type_generics);
        });
    }
}

void assert_types_equal(Program* program, Module* module, TypeValue* tv1, TypeValue* tv2, Span span, GenericKeys* func_generics, GenericKeys* type_generics) {
    if (tv1->def == NULL) resolve_typevalue(program,  module, tv1, func_generics, type_generics);
    if (tv2->def == NULL) resolve_typevalue(program, module, tv2, func_generics, type_generics);
    Span span1 = from_points(&tv1->name->elements.elements[0]->span.left, &tv1->name->elements.elements[tv1->name->elements.length-1]->span.right);
    Span span2 = from_points(&tv2->name->elements.elements[0]->span.left, &tv2->name->elements.elements[tv2->name->elements.length-1]->span.right);
    if (tv1->def != tv2->def) {
        spanned_error("Types do not match", span, "%s @ %s and %s @ %s", 
            to_str_writer(stream, fprint_typevalue(stream, tv1)), to_str_writer(stream, fprint_span(stream, &span1)),
            to_str_writer(stream, fprint_typevalue(stream, tv2)), to_str_writer(stream, fprint_span(stream, &span2))
        );
    }
    if (tv1->generics != NULL && tv2->generics != NULL) {
        if (tv1->generics->generics.length != tv2->generics->generics.length) {
            spanned_error("Types do not match", span, "%s @ %s and %s @ %s", 
                to_str_writer(stream, fprint_typevalue(stream, tv1)), to_str_writer(stream, fprint_span(stream, &span1)),
                to_str_writer(stream, fprint_typevalue(stream, tv2)), to_str_writer(stream, fprint_span(stream, &span2))
            );
        }
        for (usize i = 0;i < tv1->generics->generics.length;i++) {
            assert_types_equal(program, module, tv1->generics->generics.elements[i], tv2->generics->generics.elements[i], span, func_generics, type_generics);
        }
    } else {
        if (tv1->generics != NULL || tv2->generics != NULL) {
            spanned_error("Types do not match", span, "%s @ %s and %s @ %s", 
                to_str_writer(stream, fprint_typevalue(stream, tv1)), to_str_writer(stream, fprint_span(stream, &span1)),
                to_str_writer(stream, fprint_typevalue(stream, tv2)), to_str_writer(stream, fprint_span(stream, &span2))
            );
        }
    }
}
void patch_generics(TypeValue* template, TypeValue* match, TypeValue* value, GenericValues* type_generics, GenericValues* func_generics);
TypeValue* replace_generic(TypeValue* tv, GenericValues* type_ctx, GenericValues* func_ctx);
static void var_find(Program* program, Module* module, VarList* vars, Variable* var, GenericKeys* func_generics, GenericKeys* type_generics) {
    Path* path = var->path;
    Identifier* name = path->elements.elements[path->elements.length-1];
    if (!path->absolute && path->elements.length == 1) {
        for (int i = vars->length-1;i >= 0;i--) {
            if (str_eq(vars->elements[i]->name, name->name)) {
                var->box = vars->elements[i];
                var->static_ref = NULL;
                if (var->values != NULL) spanned_error("Generic variable?", var->path->elements.elements[0]->span, "Variable values are not supposed to be generic");
                return;
            }
        }
    }
    ModuleItem* item = resolve_item(program, module, path, MIT_ANY, func_generics, type_generics, &var->values);
    VarBox* box = malloc(sizeof(VarBox));
    box->id = ~0;
    box->name = NULL;
    box->mi = item;
    box->values = var->values;
    var->box = box;
    switch (item->type) {
        case MIT_FUNCTION: {
            FuncDef* func = item->item;
            //if (func->generics != NULL && func->generics->generics.length > 0) spanned_error("Generic function literal", var->path->elements.elements[0]->span, "Title says it all. Only use ungeneric functions here.");
            if (func->generics != NULL) {
                if (var->values == NULL) spanned_error("Generic function", var->path->elements.elements[0]->span, "this function is generic. please provide generic args.");
                if (func->generics->generics.length != var->values->generics.length) spanned_error("Mismatch generic count", var->path->elements.elements[0]->span, "title says it all, todo better errors in this entire area...");
            } else if (var->values != NULL) spanned_error("Ungeneric function", var->path->elements.elements[0]->span, "this function is not generic");
            TypeValue* tv = gen_typevalue("std::function_ptr<T>", &name->span);
            resolve_typevalue(program, module, func->return_type, NULL, NULL);
            TypeValue* ret = replace_generic(func->return_type, NULL, var->values);
            tv->generics->generics.elements[0] = ret;
            box->resolved = malloc(sizeof(TVBox));
            box->resolved->type = tv;
            register_item(NULL, var->values, func->generics);
        } break;
        case MIT_STATIC: {
            if (var->values != NULL) spanned_error("Generic static?", var->path->elements.elements[0]->span, "Static values are not supposed to be generic (yet?!)");
            Static* s = item->item;
            resolve_typevalue(program, module, s->type, NULL, NULL);
            box->resolved = malloc(sizeof(TVBox));
            box->resolved->type = s->type;
            var->static_ref = s;
        } break;
        case MIT_STRUCT:
            spanned_error("Expected variable", name->span, "Expected variable or function ref, got struct");
            break;
        case MIT_MODULE:
            spanned_error("Expected variable", name->span, "Expected variable or function ref, got module");
            break;
        case MIT_ANY:
            unreachable();
    }
    
}
static VarBox* var_register(VarList* vars, Variable* var) {
    Identifier* name = var->path->elements.elements[var->path->elements.length-1];
    if (var->path->absolute || var->path->elements.length != 1) spanned_error("Variable has too much path", name->span, "This should look more like a single variable and should have been caught earlier, that's a compiler bug.");
    VarBox* v = malloc(sizeof(VarBox));
    v->name = name->name;
    v->id = vars->length;
    v->resolved = malloc(sizeof(TVBox));
    v->resolved->type = NULL;
    v->mi = NULL;
    v->values = NULL;
    var->box = v;
    list_append(vars, v);
    return v;
}

TypeValue* replace_generic(TypeValue* tv, GenericValues* type_ctx, GenericValues* func_ctx) {
    if (type_ctx == NULL && func_ctx == NULL) return tv;
    if (tv->generics != NULL) {
        TypeValue* clone = malloc(sizeof(TypeValue));
        clone->def = tv->def;
        clone->name = tv->name;
        clone->ctx = tv->ctx;
        clone->generics = malloc(sizeof(GenericValues));
        clone->generics->span = tv->generics->span;
        clone->generics->generics = list_new(TypeValueList);
        clone->generics->resolved = map_new();
        Map* rev = map_new();
        map_foreach(tv->generics->resolved, str key, TypeValue* val, {
            map_put(rev, to_str_writer(s, fprintf(s, "%p", val)), key);
        });
        list_foreach(&tv->generics->generics, i, TypeValue* val, {
            str real_key = map_get(rev, to_str_writer(s, fprintf(s, "%p", val)));
            val = replace_generic(val, type_ctx, func_ctx);
            list_append(&clone->generics->generics, val);
            map_put(clone->generics->resolved, real_key, val);
        });
        return clone;
    }
    if (tv->name->absolute || tv->name->elements.length != 1) return tv;
    str name = tv->name->elements.elements[0]->name;
    TypeValue* type_candidate = NULL;
    if (type_ctx != NULL) type_candidate = map_get(type_ctx->resolved, name);
    TypeValue* func_candidate = NULL;
    if (func_ctx != NULL) func_candidate = map_get(func_ctx->resolved, name);
    if (type_candidate != NULL && func_candidate != NULL) {
        spanned_error("Multiple generic candidates", tv->name->elements.elements[0]->span, "Generic `%s` could be replaced by %s @ %s or %s @ %s", name,
            to_str_writer(s, fprint_typevalue(s, type_candidate)), to_str_writer(s, fprint_span(s, &type_candidate->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_typevalue(s, func_candidate)), to_str_writer(s, fprint_span(s, &func_candidate->name->elements.elements[0]->span)));
    }
    if (type_candidate != NULL) return type_candidate;
    if (func_candidate != NULL) return func_candidate;
    return tv;
}

void patch_tvs(TypeValue** tv1ref, TypeValue** tv2ref) {
    TypeValue* tv1 = *tv1ref;
    TypeValue* tv2 = *tv2ref;
    // log("patching %s and %s", to_str_writer(s, fprint_typevalue(s, tv1)), to_str_writer(s, fprint_typevalue(s, tv2)));
    if (tv1 == NULL || (!tv1->name->absolute && tv1->name->elements.length == 1 && str_eq(tv1->name->elements.elements[0]->name, "_"))) {
        *tv1ref = tv2;
        return;
    }
    if (tv2 == NULL || (!tv2->name->absolute && tv2->name->elements.length == 1 && str_eq(tv2->name->elements.elements[0]->name, "_"))) {
        *tv2ref = tv1;
        return;
    }
    if (tv1->generics != NULL && tv2->generics != NULL) {
        if (tv1->generics->generics.length == tv2->generics->generics.length) {
            Map* rev1 = map_new();
            map_foreach(tv1->generics->resolved, str key, TypeValue* val, {
                map_put(rev1, to_str_writer(s, fprintf(s, "%p", val)), key);
            });
            Map* rev2 = map_new();
            map_foreach(tv2->generics->resolved, str key, TypeValue* val, {
                map_put(rev2, to_str_writer(s, fprintf(s, "%p", val)), key);
            });
            for (usize i = 0;i < tv1->generics->generics.length;i++) {
                TypeValue* oldtv1_g = tv1->generics->generics.elements[i];
                TypeValue* oldtv2_g = tv2->generics->generics.elements[i];
                patch_tvs(&tv1->generics->generics.elements[i], &tv2->generics->generics.elements[i]);
                if (tv1->generics->generics.elements[i] != oldtv1_g) {
                    str key = map_get(rev1, to_str_writer(s, fprintf(s, "%p", oldtv1_g)));
                    map_put(tv1->generics->resolved, key, tv1->generics->generics.elements[i]);
                }
                if (tv2->generics->generics.elements[i] != oldtv2_g) {
                    str key = map_get(rev2, to_str_writer(s, fprintf(s, "%p", oldtv2_g)));
                    map_put(tv2->generics->resolved, key, tv2->generics->generics.elements[i]);
                }
            }
            return;
        }
    }
    if (tv1->generics == NULL && tv2->generics == NULL) {
        return;
    }
    panic("%s @ %s and %s @ %s do not match in generics", 
        to_str_writer(s, fprint_typevalue(s, tv1)), to_str_writer(s, fprint_span(s, &tv1->name->elements.elements[0]->span)),
        to_str_writer(s, fprint_typevalue(s, tv2)), to_str_writer(s, fprint_span(s, &tv2->name->elements.elements[0]->span))
    );
}

static void r_collect_generics(TypeValue* template, TypeValue* match, GenericKeys* keys, GenericValues* gvals) {
    if ((template->generics == NULL && match->generics != NULL) || (template->generics != NULL && match->generics == NULL)) {
        spanned_error("Type structure mismatch", match->name->elements.elements[0]->span, "Type %s @ %s does not structurally match its tempate %s @ %s (generic/ungeneric)",
            to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &match->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_typevalue(s, template)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)));
    }
    if (template->generics == NULL) { // match->generics also NULL
        // is generic param?
        if (template->name->elements.length == 1 && map_contains(keys->resolved, template->name->elements.elements[0]->name)) {
            str name = template->name->elements.elements[0]->name;
            list_foreach(&keys->generics, i, Identifier* ident, {
                if (str_eq(ident->name, name)) {
                    gvals->generics.elements[i] = match;
                    map_put(gvals->resolved, name, match);
                    break;
                }
            });
        }
        return;
    }
    if (template->generics->generics.length != match->generics->generics.length) {
        spanned_error("Type structure mismatch", match->name->elements.elements[0]->span, "Type %s @ %s does not structurally match its tempate %s @ %s (generic item mismatch)",
            to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &match->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_typevalue(s, template)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)));
    }
    list_foreach(&template->generics->generics, i, TypeValue* template_g, {
        TypeValue* match_g = match->generics->generics.elements[i];
        r_collect_generics(template_g, match_g, keys, gvals);
    });
}

// This function is specifically made for methods and impl blocks
// impl<keys> template { ... }
// match::foo(1, 2, 3);
//
// returns: <T=i32, U=u32>
// Error when unable to fill all generic names
//
//                              Foo<T, U>            Foo<i32, u32>     <T, U>
GenericValues* collect_generics(TypeValue* template, TypeValue* match, GenericKeys* keys) {
    GenericValues* gvals = malloc(sizeof(GenericValues));
    gvals->generics = list_new(TypeValueList);
    gvals->resolved = map_new();
    if (keys == NULL) return gvals;
    list_foreach(&keys->generics, i, Identifier* ident, {
        UNUSED(ident);
        list_append(&gvals->generics, NULL); // insert dummy for now
    });
    r_collect_generics(template, match, keys, gvals);
    list_foreach(&keys->generics, i, Identifier* ident, {
        UNUSED(ident);
        if (gvals->generics.elements[i] == NULL) {
            spanned_error("Unbound generic", ident->span, "Generic `%s` was not bound by %s @ %s and as such could not be filled by %s @ %s", 
                ident->name, 
                to_str_writer(s, fprint_typevalue(s, template)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)),
                to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &match->name->elements.elements[0]->span)));
        }
    });
    return gvals;
}

//                  Foo<_>               Foo<T>            Foo<i32>          <T, U, V>                     <T, U, V>
void patch_generics(TypeValue* template, TypeValue* match, TypeValue* value, GenericValues* type_generics, GenericValues* func_generics) {
    if (!template->name->absolute && template->name->elements.length == 1 && str_eq(template->name->elements.elements[0]->name, "_")) {
        if (!match->name->absolute && match->name->elements.length == 1) {
            str generic = match->name->elements.elements[0]->name;
            if (type_generics == NULL && func_generics == NULL) spanned_error("No generics supplied", value->name->elements.elements[0]->span, "Expected generics, got none. %s @ %s, %s @ %s and %s @ %s", 
                to_str_writer(s, fprint_typevalue(s, template)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)),
                to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &match->name->elements.elements[0]->span)),
                to_str_writer(s, fprint_typevalue(s, value)), to_str_writer(s, fprint_span(s, &value->name->elements.elements[0]->span)));
            TypeValue* type_candidate = NULL;
            if (type_generics != NULL) type_candidate = map_get(type_generics->resolved, generic);
            TypeValue* func_candidate = NULL;;
            if (func_generics != NULL) func_candidate = map_get(func_generics->resolved, generic);
            if (type_candidate == NULL && func_candidate == NULL) {
                spanned_error("No such generic", match->name->elements.elements[0]->span, "%s is not a valid generic for %s @ %s or %s @ %s", to_str_writer(s, fprint_typevalue(s, match)), 
                    to_str_writer(s, fprint_span_contents(s, &type_generics->span)), to_str_writer(s, fprint_span(s, &type_generics->span)),
                    to_str_writer(s, fprint_span_contents(s, &func_generics->span)), to_str_writer(s, fprint_span(s, &func_generics->span))
                );
            }
            if (type_candidate != NULL && func_candidate != NULL) {
                spanned_error("Ambigous generic", match->name->elements.elements[0]->span, "%s culd be matched both by %s @ %s or %s @ %s", to_str_writer(s, fprint_typevalue(s, match)), 
                    to_str_writer(s, fprint_span_contents(s, &type_generics->span)), to_str_writer(s, fprint_span(s, &type_generics->span)),
                    to_str_writer(s, fprint_span_contents(s, &func_generics->span)), to_str_writer(s, fprint_span(s, &func_generics->span))
                );
            }
            if (type_candidate != NULL) {
                TypeValue* old = map_put(type_generics->resolved, generic, value);
                list_find_map(&type_generics->generics, i, TypeValue** tvref, *tvref == old, {
                    *tvref = value;
                });
            }
            if (func_candidate != NULL) {
                TypeValue* old = map_put(func_generics->resolved, generic, value);
                list_find_map(&func_generics->generics, i, TypeValue** tvref, *tvref == old, {
                    *tvref = value;
                });
            }
            return;
        }
        spanned_error("Not a generic", match->name->elements.elements[0]->span, "%s is not a generic parameter and cannot substitute _ @ %s", to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)));
    }
    if (!match->name->absolute && match->name->elements.length == 1 && match->generics == NULL) return;
    if (template->generics != NULL && match->generics != NULL && value->generics != NULL) {
        if (template->generics->generics.length == match->generics->generics.length && match->generics->generics.length == value->generics->generics.length) {
            for (usize i = 0;i < template->generics->generics.length;i++) {
                patch_generics(template->generics->generics.elements[i], match->generics->generics.elements[i], value->generics->generics.elements[i], type_generics, func_generics);
            }
            return;
        }
    }
    if (template->generics == NULL && match->generics == NULL && value->generics == NULL) {
        return;
    }
    panic("%s @ %s, %s @ %s and %s @ %s do not match in generics", 
        to_str_writer(s, fprint_typevalue(s, template)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)),
        to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &match->name->elements.elements[0]->span)),
        to_str_writer(s, fprint_typevalue(s, value)), to_str_writer(s, fprint_span(s, &value->name->elements.elements[0]->span))
    );
}

TVBox* new_tvbox() {
    TVBox* box = malloc(sizeof(TVBox));
    box->type = NULL;
    return box;
}

void fill_tvbox(Program *program, Module *module, Span span, GenericKeys* func_generics, GenericKeys* type_generics, TVBox* box, TypeValue* value) {
    if (value->def == NULL) resolve_typevalue(program, module, value, func_generics, type_generics);
    if (box->type != NULL) {
        patch_tvs(&box->type, &value);
        assert_types_equal(program, module, box->type, value, span, func_generics, type_generics);
    } else {
        box->type = value;
    }
}

static bool satisfies_impl_bounds_r(TypeValue* tv, TypeValue* impl_tv) {
    if (impl_tv->def->module == NULL) return true; // free generic
    if (tv->def != impl_tv->def) return false; // type mismatch
    if ((tv->generics == NULL || tv->generics->generics.length == 0) && (impl_tv->generics == NULL || impl_tv->generics->generics.length == 0)) return true; // no generics
    if (tv->generics->generics.length != impl_tv->generics->generics.length) return false;
    for(usize i = 0; i < tv->generics->generics.length; i++) {
        if (!satisfies_impl_bounds_r(tv->generics->generics.elements[i], impl_tv->generics->generics.elements[i])) return false;
    }
    return true;
}
static bool satisfies_impl_bounds(TypeValue* tv, MethodImpl* impl) {
    return satisfies_impl_bounds_r(tv, impl->tv);
}

MethodImpl* resolve_method_instance(Program* program, TypeValue* tv, Identifier* name) {
    Module* mod = tv->def->module;
    if (mod == NULL) spanned_error("Invalid type", name->span, "Cannot call method `%s` on type %s as it is a generic parameter.", name->name, to_str_writer(s, fprint_typevalue(s, tv)));
    MethodImplList* list = map_get(mod->package_method_map, name->name);
    Module* base = tv->def->module;
    while (base->parent != NULL) base = base->parent;
    if (list == NULL || list->length == 0) spanned_error("No such method", name->span, "No such method `%s` defined on any type in package `%s` (on %s).", name->name, base->name->name,  to_str_writer(s, fprint_typevalue(s, tv)));
    MethodImpl* chosen = NULL;
    for (usize i = 0;i < list->length; i++) {
        MethodImpl* impl = &list->elements[i];
        if (satisfies_impl_bounds(tv, impl)) {
            if (chosen != NULL) spanned_error("Multiple method candiates", tv->name->elements.elements[0]->span, "Multiple candiates found for %s::%s\n#1: %s::%s @ %s\n#2: %s::%s @ %s", 
                to_str_writer(s, fprint_typevalue(s, tv)), name->name,
                to_str_writer(s, fprint_typevalue(s, impl->tv)), impl->func->name->name, to_str_writer(s, fprint_span(s, &impl->func->name->span)),
                to_str_writer(s, fprint_typevalue(s, chosen->tv)), chosen->func->name->name, to_str_writer(s, fprint_span(s, &chosen->func->name->span)));
            chosen = impl;
        }
    }
    if (chosen == NULL) spanned_error("No such method", name->span, "No such method `%s` on type %s @ %s defined in package `%s`.", 
        name->name, to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &tv->def->name->span)), base->name->name);
    if (!chosen->func->head_resolved) {
        resolve_funcdef(program, chosen->func, chosen->keys);
    }
    return chosen;
}

void finish_tvbox(TVBox* box, GenericValues* type_values) {
    register_item(type_values, box->type->generics, box->type->def->generics);
}

void resolve_block(Program* program, FuncDef* func, GenericKeys* type_generics, Block* block, VarList* vars, TVBox* t_return);
void resolve_expr(Program* program, FuncDef* func, GenericKeys* type_generics, Expression* expr, VarList* vars, TVBox* t_return) {
    expr->resolved = t_return;
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            TVBox* op_arg_box;
            if (str_eq(op->op, ">") || str_eq(op->op, "<") || str_eq(op->op, ">=") || str_eq(op->op, "<=") || str_eq(op->op, "!=") || str_eq(op->op, "==")) {
                op_arg_box = new_tvbox();
                TypeValue* bool_ty = gen_typevalue("::intrinsics::types::bool", &op->op_span);
                resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
                fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, bool_ty);
            } else {
                op_arg_box = t_return;
            }
            resolve_expr(program, func, type_generics, op->lhs, vars, op_arg_box);
            resolve_expr(program, func, type_generics, op->rhs, vars, op_arg_box);
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncDef* fd = resolve_item(program, func->module, fc->name, MIT_FUNCTION, func->generics, type_generics, &fc->generics);
            // preresolve return
            TypeValue* pre_ret = replace_generic(fd->return_type, NULL, fc->generics);
            if (t_return->type != NULL) patch_generics(pre_ret, fd->return_type, t_return->type, NULL, fc->generics);

            // checking arg count
            fc->def = fd;
            if (fd->is_variadic) {
                if (fd->args.length > fc->arguments.length) spanned_error("Too few args for variadic function", expr->span, "expected at least %llu args, got %llu", fd->args.length, fc->arguments.length);
            } else {
                if (fd->args.length != fc->arguments.length) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", fd->args.length, fc->arguments.length);
            }
            // resolving args
            list_foreach(&fc->arguments, i, Expression* arg, {
                if (i < fd->args.length) {
                    TVBox* argbox = new_tvbox();
                    TypeValue* arg_tv = replace_generic(fd->args.elements[i]->type, NULL, fc->generics);
                    argbox->type = arg_tv;
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    patch_generics(replace_generic(fd->args.elements[i]->type, NULL, fc->generics), fd->args.elements[i]->type, argbox->type, NULL, fc->generics);
                    finish_tvbox(argbox, NULL);
                } else { // variadic arg
                    TVBox* argbox = new_tvbox();
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    finish_tvbox(argbox, NULL);
                }
            });

            TypeValue* ret = replace_generic(fd->return_type, NULL, fc->generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, ret);

            register_item(NULL, fc->generics, fd->generics);        
        } break;
        case EXPR_METHOD_CALL: {
            MethodCall* call = expr->expr;
            TVBox* objectbox = new_tvbox();
            resolve_expr(program, func, type_generics, call->object, vars, objectbox);

            TypeValue* ptr = gen_typevalue("::intrinsics::types::ptr<_>", &expr->span);
            resolve_typevalue(program, func->module, ptr, NULL, NULL);
            TypeValue* method_type = call->object->resolved->type;
            bool deref_to_call = false;
            if (method_type->def == ptr->def) {
                method_type = method_type->generics->generics.elements[0];
                deref_to_call = true;
            }
            MethodImpl* method = resolve_method_instance(program, method_type, call->name);
            call->def = method;
            if (call->generics != NULL) {
                for (usize i = 0; i < call->generics->generics.length;i++) {
                    Identifier* generic_key = method->func->generics->generics.elements[i];
                    TypeValue* generic_value = call->generics->generics.elements[i];
                    resolve_typevalue(program, func->module, generic_value, func->generics, type_generics);
                    map_put(call->generics->resolved, generic_key->name, generic_value);
                }
            } else if(method->func->generics != NULL) {
                call->generics = malloc(sizeof(GenericValues));
                call->generics->span = expr->span;
                call->generics->generics = list_new(TypeValueList);
                call->generics->resolved = map_new();
                list_foreach(&method->func->generics->generics, i, Identifier* generic, {
                    TypeValue* dummy = gen_typevalue("_", &expr->span);
                    list_append(&call->generics->generics, dummy);
                    map_put(call->generics->resolved, generic->name, dummy);
                });
            }

            if (call->def->func->args.length == 0) spanned_error("Method call on zero arg method", expr->span, "Expected at least one argument (the calling object %s), but the method defines zero.\nTry calling statically as %s::<>::%s()", 
                to_str_writer(s, fprint_typevalue(s, call->object->resolved->type)), to_str_writer(s, fprint_typevalue(s, call->object->resolved->type)), method->func->name->name);
            if (call->def->func->is_variadic) {
                if (call->def->func->args.length > call->arguments.length + 1) spanned_error("Too few args for variadic method", expr->span, "expected at least %llu args, got %llu", call->def->func->args.length - 1, call->arguments.length);
            } else {
                if (call->def->func->args.length != call->arguments.length + 1) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", call->def->func->args.length - 1, call->arguments.length);
            }

            TypeValue* arg_tv = call->def->func->args.elements[0]->type;
            TypeValue* called_type = NULL;

            if (ptr->def == arg_tv->def) { // arg is a ptr
                if (!deref_to_call) { // we dont have a ptr yet
                    Expression* inner = call->object;
                    call->object = malloc(sizeof(Expression));
                    call->object->type = EXPR_TAKEREF;
                    call->object->span = inner->span;
                    call->object->expr = inner;

                    objectbox = new_tvbox();
                    resolve_expr(program, func, type_generics, call->object, vars, objectbox);
                }
                called_type = call->object->resolved->type->generics->generics.elements[0]; // get bone marrow of pointer
            } else { // arg is not a ptr
                if (deref_to_call) { // we do have a ptr
                    Expression* inner = call->object;
                    call->object = malloc(sizeof(Expression));
                    call->object->type = EXPR_DEREF;
                    call->object->span = inner->span;
                    call->object->expr = inner;

                    objectbox = new_tvbox();
                    resolve_expr(program, func, type_generics, call->object, vars, objectbox);
                }
                called_type = call->object->resolved->type;
            }
            GenericValues* type_call_vals = collect_generics(method->tv, called_type, method->keys);
            call->impl_vals = type_call_vals;

            patch_generics(replace_generic(call->def->func->args.elements[0]->type, type_call_vals, call->generics), call->def->func->args.elements[0]->type, objectbox->type, NULL, call->generics);
            finish_tvbox(objectbox, NULL);
            
            // preresolve return
            TypeValue* pre_ret = replace_generic(call->def->func->return_type, type_call_vals, call->generics);
            if (t_return->type != NULL) patch_generics(pre_ret, call->def->func->return_type, t_return->type, type_call_vals, call->generics);

            // resolving args
            list_foreach(&call->arguments, i, Expression* arg, {
                if (i + 1 < call->def->func->args.length) {
                    TVBox* argbox = new_tvbox();
                    TypeValue* arg_tv = replace_generic(call->def->func->args.elements[i+1]->type, type_call_vals, call->generics);
                    argbox->type = arg_tv;
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    patch_generics(replace_generic(call->def->func->args.elements[i+1]->type, type_call_vals, call->generics), call->def->func->args.elements[i+1]->type, argbox->type, type_call_vals, call->generics);
                    finish_tvbox(argbox, NULL);
                } else { // variadic arg
                    TVBox* argbox = new_tvbox();
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    finish_tvbox(argbox, NULL);
                }
            });

            TypeValue* ret = replace_generic(call->def->func->return_type, type_call_vals, call->generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, ret);

            register_item(type_call_vals, call->generics, call->def->func->generics);    
        } break;
        case EXPR_STATIC_METHOD_CALL: {
            StaticMethodCall* call = expr->expr;
            resolve_typevalue(program, func->module, call->tv, func->generics, type_generics);
            MethodImpl* method = resolve_method_instance(program, call->tv, call->name);
            call->def = method;
            if (call->generics != NULL) {
                for (usize i = 0; i < call->generics->generics.length;i++) {
                    Identifier* generic_key = method->func->generics->generics.elements[i];
                    TypeValue* generic_value = call->generics->generics.elements[i];
                    resolve_typevalue(program, func->module, generic_value, func->generics, type_generics);
                    map_put(call->generics->resolved, generic_key->name, generic_value);
                }
            } else if(method->func->generics != NULL) {
                call->generics = malloc(sizeof(GenericValues));
                call->generics->span = expr->span;
                call->generics->generics = list_new(TypeValueList);
                call->generics->resolved = map_new();
                list_foreach(&method->func->generics->generics, i, Identifier* generic, {
                    TypeValue* dummy = gen_typevalue("_", &expr->span);
                    list_append(&call->generics->generics, dummy);
                    map_put(call->generics->resolved, generic->name, dummy);
                });
            }
            GenericValues* type_call_vals = collect_generics(method->tv, call->tv, method->keys);
            call->impl_vals = type_call_vals;

            // preresolve return
            TypeValue* pre_ret = replace_generic(call->def->func->return_type, type_call_vals, call->generics);
            if (t_return->type != NULL) patch_generics(pre_ret, call->def->func->return_type, t_return->type, type_call_vals, call->generics);

            if (call->def->func->is_variadic) {
                if (call->def->func->args.length > call->arguments.length) spanned_error("Too few args for variadic method", expr->span, "expected at least %llu args, got %llu", call->def->func->args.length, call->arguments.length);
            } else {
                if (call->def->func->args.length != call->arguments.length) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", call->def->func->args.length, call->arguments.length);
            }

            // resolving args
            list_foreach(&call->arguments, i, Expression* arg, {
                if (i < call->def->func->args.length) {
                    TVBox* argbox = new_tvbox();
                    TypeValue* arg_tv = replace_generic(call->def->func->args.elements[i]->type, type_call_vals, call->generics);
                    argbox->type = arg_tv;
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    patch_generics(replace_generic(call->def->func->args.elements[i]->type, type_call_vals, call->generics), call->def->func->args.elements[i]->type, argbox->type, type_call_vals, call->generics);
                    finish_tvbox(argbox, NULL);
                } else { // variadic arg
                    TVBox* argbox = new_tvbox();
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    finish_tvbox(argbox, NULL);
                }
            });

            TypeValue* ret = replace_generic(call->def->func->return_type, type_call_vals, call->generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, ret);

            register_item(type_call_vals, call->generics, call->def->func->generics);  
        } break;
        case EXPR_DYN_RAW_CALL: {
            DynRawCall* call = expr->expr;

            TVBox* dyncall = new_tvbox();
            dyncall->type = gen_typevalue("::intrinsics::types::function_ptr<_>", &expr->span);
            dyncall->type->generics->generics.elements[0] = t_return->type;
            resolve_typevalue(program, func->module, dyncall->type, func->generics, type_generics);
            resolve_expr(program, func, type_generics, call->callee, vars, dyncall);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, dyncall->type->generics->generics.elements[0]);
            // resolving args, no typehints here, unsafe yada yada
            list_foreach(&call->args, i, Expression* arg, {
                TVBox* argbox = new_tvbox();
                resolve_expr(program, func, type_generics, arg, vars, argbox);
                finish_tvbox(argbox, NULL);
            });
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING: {
                    t_return->type = gen_typevalue("::intrinsics::types::c_str", &expr->span);
                    resolve_typevalue(program, func->module, t_return->type, func->generics, type_generics);
                } break;
                case NUMERAL: {
                    str num = lit->string;
                    str ty = "i32";
                    bool default_ty = true;
                    for (usize i = 0;i < strlen(num);i++) {
                        if (num[i] == 'u' || num[i] == 'i') { ty = num + i; default_ty = false; break; }
                    }
                    TypeValue* tv = t_return->type;
                    if (tv == NULL || tv->def == NULL || !default_ty) {
                        tv = gen_typevalue(to_str_writer(stream, fprintf(stream, "::intrinsics::types::%s", ty)), &expr->span);
                    }
                    fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, tv);
                } break; 
                default:
                    unreachable("invalid literal type %s", TokenType__NAMES[lit->type]);
            }
        } break;
        case EXPR_BLOCK: {
            Block* block = expr->expr;
            resolve_block(program, func, type_generics, block, vars, t_return);
        } break;
        case EXPR_VARIABLE: {
            Variable* var = expr->expr;
            var_find(program, func->module, vars, var, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, var->box->resolved->type);
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;

            var_register(vars, let->var);
            if (let->type != NULL) {
                resolve_typevalue(program, func->module, let->type, func->generics, type_generics);
                fill_tvbox(program, func->module, expr->span, func->generics, type_generics, let->var->box->resolved, let->type);
            }
            resolve_expr(program, func, type_generics, let->value, vars, let->var->box->resolved);
            patch_tvs(&let->type, &let->var->box->resolved->type);

            TypeValue* tv = gen_typevalue("::intrinsics::types::unit", &expr->span);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, tv);
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            TVBox* assign_t = new_tvbox();
            resolve_expr(program, func, type_generics, assign->asignee, vars, assign_t);
            resolve_expr(program, func, type_generics, assign->value, vars, assign_t);

            TypeValue* tv = gen_typevalue("::intrinsics::types::unit", &expr->span);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, tv);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;

            TypeValue* bool_ty = gen_typevalue("::intrinsics::types::bool", &cond->cond->span);
            resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
            TVBox* cond_ty = new_tvbox();
            cond_ty->type = bool_ty;
            resolve_expr(program, func, type_generics, cond->cond, vars, cond_ty);

            if (cond->otherwise == NULL) {
                TypeValue* unit_ty = gen_typevalue("::intrinsics::types::unit", &cond->cond->span);
                resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
                fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);
            }
            resolve_block(program, func, type_generics, cond->then, vars, t_return);
            if (cond->otherwise != NULL) {
                resolve_block(program, func, type_generics, cond->otherwise, vars, t_return);
            }
        } break;
        case EXPR_WHILE_LOOP: {
            WhileLoop* wl = expr->expr;
            TypeValue* bool_ty = gen_typevalue("::intrinsics::types::bool", &wl->cond->span);
            resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
            TVBox* condbox = new_tvbox();
            condbox->type = bool_ty;
            resolve_expr(program, func, type_generics, wl->cond, vars, condbox);
            TypeValue* unit_ty = gen_typevalue("::intrinsics::types::unit", &wl->body->span);
            resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
            TVBox* bodybox = new_tvbox();
            bodybox->type = unit_ty;
            resolve_block(program, func, type_generics, wl->body, vars, bodybox);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);            
        } break;
        case EXPR_RETURN: {
            Expression* ret = expr->expr;
            TypeValue* unit_ty = gen_typevalue("::intrinsics::types::unit", &expr->span);
            resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
            if (ret == NULL) {
                // we return unit
                assert_types_equal(program, func->module, func->return_type, unit_ty, expr->span, func->generics, type_generics);
            } else {
                TVBox* return_type = new_tvbox();
                return_type->type = func->return_type;
                resolve_expr(program, func, type_generics, ret, vars, return_type);
            }
            // expression yields unit
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);            
        } break;
        case EXPR_BREAK: {
            if (expr->expr != NULL) {
                resolve_expr(program, func, type_generics, expr->expr, vars, t_return);
                todo("XPR_BREAK with expr");
            }
            TypeValue* unit_ty = gen_typevalue("::intrinsics::types::unit", &expr->span);
            resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);    
        } break;
        case EXPR_CONTINUE: {
            TypeValue* unit_ty = gen_typevalue("::intrinsics::types::unit", &expr->span);
            resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);            
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            TVBox* object_type = new_tvbox();
            resolve_expr(program, func, type_generics, fa->object, vars, object_type);
            TypeValue* reference = gen_typevalue("::intrinsics::types::ptr<_>", &expr->span);
            resolve_typevalue(program, func->module, reference, NULL, NULL);
            if (reference->def == fa->object->resolved->type->def) { // is a reference;
                Expression* inner = fa->object;
                fa->object = malloc(sizeof(Expression));
                fa->object->type = EXPR_DEREF;
                fa->object->span = inner->span;
                fa->object->expr = inner;
                TVBox* object_type = new_tvbox();
                resolve_expr(program, func, type_generics, fa->object, vars, object_type);
            }
            finish_tvbox(object_type, NULL);
            TypeValue* tv = fa->object->resolved->type;
            TypeDef* td = tv->def;
            if (td->fields == NULL) {
                if (td->module != NULL) unreachable("Fields should not be null unless it's a generic");
                spanned_error("Invalid struct field", fa->field->span, "%s @ %s has no such field '%s'. You are possibly trying to access a field from a generic parameter",  to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(stream, fprint_span(stream, &td->name->span)), fa->field->name);
            }
            Field* field = map_get(td->fields, fa->field->name);
            if (field == NULL) {
                if (td->module == NULL) unreachable("Module should not be null unless it's a generic");
                str path = to_str_writer(stream, {
                    fprint_path(stream, td->module->path);
                    fprintf(stream, "::%s", td->name->name);
                });
                if (str_eq(path, "::intrinsics::types::ptr")) {
                    spanned_error("Invalid struct field", fa->field->span, "%s @ %s has no such field '%s'. Try dereferencing the ptr to get the field of the inner type: `(*ptr).%s`.", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &td->name->span)), fa->field->name, fa->field->name);
                } else {
                    spanned_error("Invalid struct field", fa->field->span, "%s @ %s has no such field '%s'", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &td->name->span)), fa->field->name);
                }
            }
            TypeValue* field_ty = replace_generic(field->type, tv->generics, NULL);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, field_ty);
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            resolve_typevalue(program, func->module, slit->type, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, slit->type);
            
            slit->type = t_return->type;
            if (t_return->type->def == NULL) spanned_error("Type could not be inferred", expr->span, "%s is not a complete type", to_str_writer(s, fprint_typevalue(s, t_return->type)));
            TypeDef* type = slit->type->def;

            Map* temp_fields = map_new();
            map_foreach(type->fields, str key, Field* field, {
                map_put(temp_fields, key, field);
            });
            map_foreach(slit->fields, str key, StructFieldLit* field, {
                Field* f = map_remove(temp_fields, key);
                if (f == NULL) spanned_error("Invalid struct field", field->name->span, "Struct %s has no such field '%s'", type->name->name, key);
                TypeValue* field_ty = replace_generic(f->type, slit->type->generics, NULL);
                TVBox* fieldbox = new_tvbox();
                fieldbox->type = field_ty;
                resolve_expr(program, func, type_generics, field->value, vars, fieldbox);
                patch_generics(replace_generic(f->type, slit->type->generics, NULL), f->type, fieldbox->type, NULL, slit->type->generics);
                finish_tvbox(fieldbox, NULL);
            });
            map_foreach(temp_fields, str key, Field* field, {
                UNUSED(field);
                spanned_error("Field not initialized", slit->type->name->elements.elements[0]->span, "Field '%s' of struct %s was not initialized", type->name->name, key);
            });
        
            register_item(NULL, slit->type->generics, type->generics);        
        } break;
        case EXPR_C_INTRINSIC: {
            CIntrinsic* ci = expr->expr;
            map_foreach(ci->var_bindings, str key, Variable* var, {
                UNUSED(key);
                var_find(program, func->module, vars, var, func->generics, type_generics);
            });
            map_foreach(ci->type_bindings, str key, TypeValue* tv, {
                UNUSED(key);
                resolve_typevalue(program, func->module, tv, func->generics, type_generics);
            });
            resolve_typevalue(program, func->module, ci->ret_ty, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, ci->ret_ty);
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            TVBox* inner_tv = new_tvbox();
            if (t_return->type != NULL) {
                TypeValue* reference = gen_typevalue("::intrinsics::types::ptr<_>", &expr->span);
                resolve_typevalue(program, func->module, reference, NULL, NULL);
                if (reference->def != t_return->type->def) spanned_error("Expected resulting type to be a reference", expr->span, "Type %s, is not a reference, expected %s as a result", to_str_writer(s, fprint_typevalue(s, t_return->type)),to_str_writer(s, fprint_typevalue(s, reference)));
                inner_tv->type = t_return->type->generics->generics.elements[0];
            }
            resolve_expr(program, func, type_generics, inner, vars, inner_tv);
            finish_tvbox(inner_tv, NULL);
            TypeValue* reference = gen_typevalue("::intrinsics::types::ptr::<_>", &expr->span);
            reference->generics->generics.elements[0] = inner->resolved->type;
            resolve_typevalue(program, func->module, reference, NULL, NULL);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, reference);
        } break;
        case EXPR_DEREF: {
            Expression* inner = expr->expr;
            TVBox* inner_tv = new_tvbox();
            if (t_return->type != NULL) {
                inner_tv->type = gen_typevalue("::intrinsics::types::ptr::<_>", &expr->span);
                inner_tv->type->generics->generics.elements[0] = t_return->type;
            }
            resolve_expr(program, func, type_generics, inner, vars, inner_tv);
            finish_tvbox(inner_tv, NULL);
            if (!str_eq(to_str_writer(s, fprint_td_path(s, inner_tv->type->def)), "::intrinsics::types::ptr")) spanned_error("Expected ptr to dereference", expr->span, "Cannot dereference type %s, expected ::intrinsics::types::ptr<_>", to_str_writer(s, fprint_typevalue(s, inner->resolved->type)));
            if (inner->resolved->type->generics == NULL || inner->resolved->type->generics->generics.length != 1) spanned_error("Expected ptr to have a pointee", expr->span, "Pointer %s should have one generic argument as its pointee", to_str_writer(s, fprint_typevalue(s, inner->resolved->type)));
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, inner->resolved->type->generics->generics.elements[0]);
        } break;
        default:
            unreachable("%s", ExprType__NAMES[expr->type]);
    }
}

void resolve_block(Program* program, FuncDef* func, GenericKeys* type_generics, Block* block, VarList* vars, TVBox* t_return) {
    usize restore_len = vars->length;
    
    list_foreach(&block->expressions, i, Expression* expr, {
        if (block->yield_last && i == block->expressions.length - 1) {
            resolve_expr(program, func, type_generics, expr, vars, t_return);
        } else {
            TVBox* expr_box = new_tvbox();
            resolve_expr(program, func, type_generics, expr, vars, expr_box);
            finish_tvbox(expr_box, NULL);
        }
    });

    TVBox* yield_ty;
    if (block->yield_last && block->expressions.length > 0) {
        yield_ty = block->expressions.elements[block->expressions.length-1]->resolved;
    } else {
        yield_ty = malloc(sizeof(TVBox));
        yield_ty->type = gen_typevalue("::intrinsics::types::unit", &block->span);
        resolve_typevalue(program, func->module, yield_ty->type, func->generics, type_generics);
    }
    block->res = yield_ty->type;
    fill_tvbox(program, func->module, block->span, func->generics, type_generics, t_return, yield_ty->type);
    
    finish_tvbox(yield_ty, NULL);
    
    vars->length = restore_len;
}

void resolve_module(Program* program, Module* module);
void resolve_funcdef(Program* program, FuncDef* func, GenericKeys* type_generics) {
    if (!func->module->resolved && !func->module->in_resolution) {
        resolve_module(program, func->module);
    }
    if (func->mt == NULL) log("Resolving function %s::%s", to_str_writer(s, fprint_path(s, func->module->path)), func->name->name);
    else log("Resolving method %s::%s", to_str_writer(s, fprint_typevalue(s, func->mt)), func->name->name);

    if (str_eq(func->name->name, "_")) spanned_error("Invalid func name", func->name->span, "`_` is a reserved name.");
    if (func->return_type == NULL) {
        func->return_type = gen_typevalue("::intrinsics::types::unit", &func->name->span);
    }
    if (func->generics != NULL) {
        list_foreach(&func->generics->generics, i, Identifier* key, {
            TypeDef* type = malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = map_new();
            type->module = NULL;
            map_put(func->generics->resolved, key->name, type);
        });
    }
    VarList vars = list_new(VarList);
    list_foreach(&func->args, i, Argument* arg, {
        VarBox* v = var_register(&vars, arg->var);
        v->resolved->type = arg->type;
        resolve_typevalue(program, func->module, arg->type, func->generics, type_generics);
    });
    resolve_typevalue(program, func->module, func->return_type, func->generics, type_generics);

    func->head_resolved = true;

    if (func->body != NULL) {
        TVBox* blockbox = new_tvbox();
        // last expression is not a "return"?
        if (func->body->expressions.length > 0) {
            Expression* last = func->body->expressions.elements[func->body->expressions.length - 1];
            if (last->type != EXPR_RETURN) blockbox->type = func->return_type;
        }
        resolve_block(program, func, type_generics, func->body, &vars, blockbox);
        finish_tvbox(blockbox, NULL);
        // last expression is not a "return"?
        if (func->body->expressions.length > 0) {
            Expression* last = func->body->expressions.elements[func->body->expressions.length - 1];
            if (last->type == EXPR_RETURN) goto return_manually;
        }
        assert_types_equal(program, func->module, func->body->res, func->return_type, func->name->span, func->generics, type_generics);
        return_manually: {}
    }
}

void resolve_typedef(Program* program, TypeDef* ty) {
    if (!ty->module->resolved && !ty->module->in_resolution) {
        resolve_module(program, ty->module);
    }
    if (ty->extern_ref != NULL) {
        log("Type %s is extern", ty->name->name);
        ty->head_resolved = true;
        return;
    }
    log("Resolving type %s::%s", to_str_writer(s, fprint_path(s, ty->module->path)), ty->name->name);
    if (str_eq(ty->name->name, "_")) spanned_error("Invalid type name", ty->name->span, "`_` is a reserved name.");
    if (ty->generics != NULL) {
        list_foreach(&ty->generics->generics, i, Identifier* key, {
            TypeDef* type = malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = NULL;
            type->module = NULL;
            map_put(ty->generics->resolved, key->name, type);
        });
    }
    ty->head_resolved = true;
    map_foreach(ty->fields, str name, Field* field, {
        UNUSED(name);
        resolve_typevalue(program, ty->module, field->type, NULL, ty->generics);
    });
}

void register_impl(Program* program, Module* module, ImplBlock* impl) {
    if (impl->generics != NULL) {
        list_foreach(&impl->generics->generics, i, Identifier* key, {
            TypeDef* type = malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = map_new();
            type->module = NULL;
            map_put(impl->generics->resolved, key->name, type);
        });
    }
    resolve_typevalue(program, module, impl->type, NULL, impl->generics);
    Module* def_root = impl->type->def->module;
    if (def_root == NULL) spanned_error("TODO: good error message", impl->type->def->name->span, "(impl on generic T)");
    while (def_root->parent != NULL) def_root = def_root->parent;
    Module* ctx_root = module;
    while (ctx_root->parent != NULL) ctx_root = ctx_root->parent;
    if (def_root != ctx_root) spanned_error("Impl on foreign type", impl->type->name->elements.elements[0]->span, "Cannot impl in %s on %s defined in package %s", ctx_root->name->name, to_str_writer(s, fprint_typevalue(s, impl->type)), def_root->name->name);
    map_foreach(impl->methods, str name, ModuleItem* mi, ({
        switch (mi->type) {
            case MIT_FUNCTION: {
                MethodImpl mimpl = { .tv=impl->type, .keys=impl->generics, .func = mi->item };
                MethodImplList* list;
                if (map_contains(module->package_method_map, name)) {
                    list = map_get(module->package_method_map, name);
                } else {
                    list = malloc(sizeof(MethodImplList));
                    *list = list_new(MethodImplList);
                    map_put(module->package_method_map, name, list);
                }
                list_append(list, mimpl);
            } break;
            default:
                unreachable("%s cant be defined inside impl block", ModuleItemType__NAMES[mi->type]);
        }
    }));
}

void resolve_impl(Program* program, Module* module, ImplBlock* impl) {
    UNUSED(module);
    map_foreach(impl->methods, str name, ModuleItem* mi, ({
        UNUSED(name);
        switch (mi->type) {
            case MIT_FUNCTION:
                resolve_funcdef(program, mi->item, impl->generics);
                break;
            default:
                unreachable("%s cant be defined inside impl block", ModuleItemType__NAMES[mi->type]);
        }
    }));
}


// ive missed this typo so often that now its become intentional
void resovle_imports(Program* program, Module* module, List* mask) {
    if (mask == NULL) {
        mask = malloc(sizeof(List));
        *mask = list_new(List);
    }
    if (list_contains(mask, i, void** m, *m == module)) return;
    list_append(mask, module);

    list_foreach(&module->imports, i, Import* import, {
        if (import->wildcard) {
            if (import->alias != NULL) unreachable("`use path::* as foo;` ... what is that supposed ot mean? Compiler error (probably)!");
            Module* container = resolve_item_raw(program, import->container, import->path, MIT_MODULE, mask)->item;
            map_foreach(container->items, str key, ModuleItem* item, {
                UNUSED(key);
                if (item->vis < import->vis) continue; // cant re-export with this visibility

                Visibility required = V_PRIVATE;
                if (item->module != import->container) required = V_LOCAL; // diferent module
                Module* item_root = item->module;
                Module* container_root = import->container;
                while (item_root->parent != NULL) item_root = item_root->parent;
                while (container_root->parent != NULL) container_root = container_root->parent;
                if (container_root != item_root) required = V_PUBLIC; // different package

                if (item->vis < required) continue; // cant import due to visibility. need to check manually here due to the star import

                ModuleItem* imported = malloc(sizeof(ModuleItem));
                imported->vis = import->vis;
                imported->type = item->type;
                imported->module = import->container;
                imported->origin = item;
                imported->item = item->item;
                imported->name = item->name;
                
                if (map_contains(module->items, item->name->name)) {
                    ModuleItem* orig = map_get(module->items, item->name->name);
                    if (orig->item == imported->item) continue;
                    spanned_error("Importing: name collision", import->path->elements.elements[0]->span, "%s is defined as %s @ %s and imported from %s @ %s", item->name->name, 
                                                                    TokenType__NAMES[orig->type], to_str_writer(s, fprint_span(s, &orig->name->span)),
                                                                    TokenType__NAMES[item->type], to_str_writer(s, fprint_span(s, &item->name->span)));
                }
                map_put(module->items, item->name->name, imported);
            });
        } else {
            ModuleItem* item = resolve_item_raw(program, import->container, import->path, MIT_ANY, mask);
            if (item->vis < import->vis) spanned_error("", import->path->elements.elements[0]->span, "Cannot reexport %s @ %s as %s while it is only declared as %s",
                    item->name->name, to_str_writer(s, fprint_span(s, &item->name->span)), Visibility__NAMES[import->vis], Visibility__NAMES[item->vis]);
            ModuleItem* imported = malloc(sizeof(ModuleItem));
            imported->vis = import->vis;
            imported->type = item->type;
            imported->module = import->container;
            imported->origin = item;
            imported->item = item->item;
            imported->name = item->name;
            Identifier* oname = item->name;
            if (import->alias != NULL) oname = import->alias;
            if (map_contains(module->items, oname->name)) {
                ModuleItem* orig = map_get(module->items, oname->name);
                if (orig->item == imported->item) continue;
                spanned_error("Importing: name collision", import->path->elements.elements[0]->span, "%s is defined as %s @ %s and imported from %s @ %s", oname->name, 
                                                                ModuleItemType__NAMES[orig->type], to_str_writer(s, fprint_span(s, &orig->name->span)),
                                                                ModuleItemType__NAMES[item->type], to_str_writer(s, fprint_span(s, &oname->span)));
            }
            map_put(module->items, oname->name, imported);
        }
    });
}

void resolve_module_imports(Program* program, Module* module) {
    str path_str = to_str_writer(stream, fprint_path(stream, module->path));
    log("resolving imports of %s", path_str);
    resovle_imports(program, module, NULL);
    map_foreach(module->items, str key, ModuleItem* item, {
        UNUSED(key);
        switch (item->type) {
            case MIT_MODULE: {
                if (item->origin == NULL) resolve_module_imports(program, item->item);
            } break;
            default: 
                break;
        }
    });
}
void resolve_module(Program* program, Module* module) {
    if (module->resolved) return;
    str path_str = to_str_writer(stream, fprint_path(stream, module->path));
    if (module->in_resolution) panic("recursion detected while resolving %s", path_str);
    module->in_resolution = true;

    log("Resolving module %s", path_str);

    // first we register every method, then we resolve the method (bodie)s, that way they can be defined in any order
    list_foreach(&module->impls, i, ImplBlock* impl, {
        register_impl(program, module, impl);
    });
    list_foreach(&module->impls, i, ImplBlock* impl, {
        resolve_impl(program, module, impl);
    });

    map_foreach(module->items, str key, ModuleItem* item, {
        UNUSED(key);
        switch (item->type) {
            case MIT_FUNCTION: {
                FuncDef* func = item->item;
                if (func->head_resolved) continue;
                resolve_funcdef(program, func, NULL);
            } break;
            case MIT_STRUCT: {
                TypeDef* type = item->item;
                if (type->head_resolved) continue;
                resolve_typedef(program, type);
            } break;
            case MIT_MODULE: {
                Module* mod = item->item;
                if (!mod->resolved && !mod->in_resolution) resolve_module(program, mod);
            } break;
            case MIT_STATIC: {
                Static* s = item->item;
                resolve_typevalue(program, module, s->type, NULL, NULL);
            } break;
            case MIT_ANY: 
                unreachable();
        }
    });

    module->resolved = true;
    module->in_resolution = false;
}

void resolve(Program* program) {
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_module_imports(program, mod);
    });
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_module(program, mod);
    });
}
