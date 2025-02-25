#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

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
#include "compiler.h"

typedef void (*resolver_fn)(Program* program, CompilerOptions* options, void* item);

typedef struct VarList {
    StackVar* elements;
    usize length;
    usize capacity;
    usize counter;
    // explained at ../docs/book/lang/raii/move.md
    usize loop_index;
    bool return_flag;
    Variable* return_move;
    bool break_flag;
    Variable* break_move;
} VarList;

void resolve_funcdef_body(Program* program, CompilerOptions* options, FuncDef* func);
FuncDef* resolve_method_instance(Program* program, CompilerOptions* options, TypeValue* tv, Identifier* name, bool do_error);
void resovle_imports(Program* program, CompilerOptions* options, Module* module, List* mask);
void resolve_generic_keys(Program* program, CompilerOptions* options, Module* module, GenericKeys* keys, GenericKeys* trait_keys, GenericKeys* func_generics, GenericKeys* type_generics);
void resolve_block(Program* program, CompilerOptions* options, FuncDef* func, Block* block, VarList* vars, TVBox* t_return);
static bool satisfies_impl_bounds(TypeValue* tv, TypeValue* impl_tv);

static void __fprint_res_tv(FILE* stream, TypeValue* tv, bool strict) {
    if (tv->def == NULL) {
        if (strict) {
            panic("TypeValue was not resolved %s @ %s", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &tv->name->elements.elements[0]->span)));
        } else {
            fprint_typevalue(stream, tv);
            return;
        }
    }
    if (tv->def->extern_ref != NULL) {
        fprintf(stream, "%s", tv->def->extern_ref);
        return;
    }
    fprintf(stream, "%s%p", tv->def->name->name, tv->def);
    if (tv->generics != NULL && tv->generics->generics.length > 0) {
        fprintf(stream, "<");
        list_foreach(&tv->generics->generics, i, TypeValue* t, {
            if (i > 0) fprintf(stream, ", ");
            __fprint_res_tv(stream, t, true);
        });
        fprintf(stream, ">");
    }
}

void fprint_res_tv(FILE* stream, TypeValue* tv) {
    __fprint_res_tv(stream, tv, true);
}

void try_fprint_res_tv(FILE* stream, TypeValue* tv) {
    __fprint_res_tv(stream, tv, false);
}

static str __gvals_to_key(GenericValues* generics, int resolved_level) {
    return to_str_writer(stream, {
        if (generics != NULL && generics->generics.length > 0) {
            fprintf(stream, "<");
            list_foreach(&generics->generics, i, TypeValue* tv, {
                if (i > 0) fprintf(stream, ", ");
                if (tv->def == NULL){
                    if (resolved_level >= 1) spanned_error("Unresolved typevalue", tv->name->elements.elements[0]->span, "%s is not resolved", to_str_writer(s, fprint_typevalue(s, tv)));
                    fprint_typevalue(stream, tv);
                } else {
                    if (tv->def->module != NULL) {
                        fprint_path(stream, tv->def->module->path);
                        fprintf(stream, "::");
                    } else if (resolved_level >= 2) spanned_error("Unexpanded generic", tv->name->elements.elements[0]->span, "Generic %s @ %s was not properly resolved", tv->def->name->name, to_str_writer(s, fprint_span(s, &tv->def->name->span))); 
                    fprintf(stream, "%s", tv->def->name->name);
                }
                
                fprintf(stream, "%s", __gvals_to_key(tv->generics, resolved_level));
                if (tv->ctx != NULL) {
                    if (resolved_level >= 2) spanned_error("Unresolved type", tv->name->elements.elements[0]->span, "Type %s @ %s is still in context. This is probably a compiler error.", tv->def->name->name, to_str_writer(s, fprint_span(s, &tv->def->name->span)));
                    fprintf(stream, "@%p<%s>", tv->ctx, to_str_writer(stream, {
                        list_foreach(&tv->ctx->generics, j, GKey* g, {
                            if (j > 0) fprintf(stream, ", ");
                            str key = g->name->name;
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

str gvals_to_dbg_key(GenericValues* generics) {
    return __gvals_to_key(generics, 0);
}

str gvals_to_key(GenericValues* generics) {
    return __gvals_to_key(generics, 1);
}

str gvals_to_c_key(GenericValues* generics) {
    return __gvals_to_key(generics, 2);
}

str tfvals_to_key(GenericValues* type_generics, GenericValues* func_generics) {
    return to_str_writer(s, {
       fprintf(s, "%s", gvals_to_key(type_generics)); 
       fprintf(s, "%s", gvals_to_key(func_generics)); 
    });
}

static ModuleItem* resolve_item_raw(Program* program, CompilerOptions* options, Module* module, Path* path, ModuleItemType kind, List* during_imports_mask) {
    Module* context_module = module;
    Identifier* name = path->elements.elements[path->elements.length-1];
    ModuleItem* result;
    usize i = 0;
    Identifier* m = path->elements.elements[0];
    ModuleItem* mod = NULL;
    if (!path->absolute) mod = map_get(module->items, m->name);
    if (mod != NULL) { // try local
        if (mod->type == MIT_MODULE && during_imports_mask != NULL) resovle_imports(program, options, mod->item, during_imports_mask);
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
            if (during_imports_mask != NULL) resovle_imports(program, options, package, during_imports_mask);
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
        if (during_imports_mask != NULL) resovle_imports(program, options, module, during_imports_mask); 
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
        if (result->vis < required) spanned_error("Lacking visibility", path->elements.elements[0]->span, "Cannot get item %s::%s @ %s while in %s as this would require it having a visibility of %s,\nbut %s was only declared as %s",
                                            to_str_writer(s, fprint_path(s, result->module->path)), result->name->name, to_str_writer(s, fprint_span(s, &result->name->span)), to_str_writer(s, fprint_path(s, context_module->path)), Visibility__NAMES[required], result->name->name, Visibility__NAMES[result->vis]);
        Module* res_tree = result->module;
        while (res_tree != NULL) {
            Module* ctx_tree = context_module;
            while (ctx_tree != NULL) { // wasteful but shush
                if (ctx_tree == res_tree) goto done_checking_tree;
                ctx_tree = ctx_tree->parent;
            }
            if (res_tree->vis < required) spanned_error("Lacking visibility", path->elements.elements[0]->span, "Cannot get item %s::%s @ %s while in %s as this would require the ancestor module %s to have a visibility of %s,\nbut %s was only declared as %s",
                                            to_str_writer(s, fprint_path(s, result->module->path)), result->name->name, to_str_writer(s, fprint_span(s, &result->name->span)), to_str_writer(s, fprint_span(s, &result->module->path->elements.elements[0]->span)), Visibility__NAMES[required], to_str_writer(s, fprint_path(s, context_module->path)), to_str_writer(s, fprint_path(s, res_tree->path)), ctx_tree->name->name, to_str_writer(s, fprint_span(s, &result->name->span)), Visibility__NAMES[res_tree->vis]);
            res_tree = res_tree->parent;
        }
        done_checking_tree: {}
    }
    
    return result;
}

void validate_item(Program* program, CompilerOptions* options, GenericValues* type_values, GenericValues* func_values, GenericKeys* tvs_keys, GenericKeys* fvs_keys, GenericKeys* gkeys) {
    if (gkeys == NULL) return;
    if (type_values != NULL) {
        if (tvs_keys != NULL && type_values->generics.length != tvs_keys->generics.length) unreachable("Compiler error: keys and values length doesnt match: %s @ %s and %s @ %s",
            to_str_writer(s, fprint_generic_keys(s, tvs_keys)), to_str_writer(s, fprint_span(s, &tvs_keys->span)),
            to_str_writer(s, fprint_generic_values(s, type_values)), to_str_writer(s, fprint_span(s, &type_values->span)));
        list_foreach(&type_values->generics, i, TypeValue* v, {
            GKey* key = tvs_keys->generics.elements[i];
            list_foreach(&key->bounds, i, TraitBound* bound, {
                if (v->def->module != NULL) { // actual type
                    if (!map_contains(v->trait_impls, to_str_writer(s, fprintf(s, "%p", bound->resolved)))) {
                        spanned_error("Unsatisfied trait bound", v->name->elements.elements[v->name->elements.length-1]->span, 
                            "Type %s does not satisfy trait bound %s::%s.\nConsider implementing this trait for this type", 
                            to_str_writer(s, fprint_type(s, v->def)), to_str_writer(s, fprint_path(s, bound->resolved->module->path)), bound->resolved->name->name);
                    }
                } else { // generic key
                    if (!list_contains(&v->def->traits, j, TraitDef* def, def == bound->resolved)) {
                    spanned_error("Unsatisfied trait bound", v->name->elements.elements[v->name->elements.length-1]->span, 
                            "Type %s does not satisfy trait bound %s::%s.\nConsider requiring this trait for this type", 
                            to_str_writer(s, fprint_type(s, v->def)), to_str_writer(s, fprint_path(s, bound->resolved->module->path)), bound->resolved->name->name);
                    }
                }
            });
            validate_item(program, options, v->generics, NULL, v->def->generics, NULL, v->def->generics); 
        });
    }

    if (func_values != NULL) {
        if (fvs_keys != NULL && func_values->generics.length != fvs_keys->generics.length) unreachable("Compiler error: keys and values length doesnt match: %s @ %s and %s @ %s",
            to_str_writer(s, fprint_generic_keys(s, fvs_keys)), to_str_writer(s, fprint_span(s, &fvs_keys->span)),
            to_str_writer(s, fprint_generic_values(s, func_values)), to_str_writer(s, fprint_span(s, &func_values->span)));
        list_foreach(&func_values->generics, i, TypeValue* v, {
            GKey* key = fvs_keys->generics.elements[i];
            list_foreach(&key->bounds, j, TraitBound* bound, {
                if (v->def->module != NULL) { // actual type
                    if (!map_contains(v->trait_impls, to_str_writer(s, fprintf(s, "%p", bound->resolved)))) {
                        spanned_error("Unsatisfied trait bound", v->name->elements.elements[v->name->elements.length-1]->span, 
                            "Type %s does not satisfy trait bound %s::%s.\nConsider implementing this trait for this type", 
                            to_str_writer(s, fprint_type(s, v->def)), to_str_writer(s, fprint_path(s, bound->resolved->module->path)), bound->resolved->name->name);
                    }
                } else { // generic key
                    if (!list_contains(&v->def->traits, j, TraitDef* def, def == bound->resolved)) {
                    spanned_error("Unsatisfied trait bound", v->name->elements.elements[v->name->elements.length-1]->span, 
                            "Type %s does not satisfy trait bound %s::%s.\nConsider requiring this trait for this type", 
                            to_str_writer(s, fprint_type(s, v->def)), to_str_writer(s, fprint_path(s, bound->resolved->module->path)), bound->resolved->name->name);
                    }
                }
            });
            validate_item(program, options, v->generics, NULL, v->def->generics, NULL, v->def->generics); 
        });
    }
}

static usize ITEM_CACHE_HOT_HITS = 0;
static usize ITEM_CACHE_COLD_HITS = 0;
static usize ITEM_CACHE_MISSES = 0;
#define HOT_CACHE_SIZE 12
#define COLD_CACHE_SIZE 12
static_assert(HOT_CACHE_SIZE > 0, "HOT_CACHE_SIZE has to be greater than zero");
static_assert(COLD_CACHE_SIZE > 0, "COLD_CACHE_SIZE has to be greater than zero");
void report_item_cache_stats() {
    info(ANSI(ANSI_BOLD, ANSI_CYAN_FG) "CACHE" ANSI_RESET_SEQUENCE, "Hot hits: %llu | Cold hits: %llu | Misses: %llu", ITEM_CACHE_HOT_HITS, ITEM_CACHE_COLD_HITS, ITEM_CACHE_MISSES);
}
void* resolve_item(Program* program, CompilerOptions* options, Module* module, Path* path, ModuleItemType kind, GenericKeys* func_generics, GenericKeys* type_generics, GenericValues** gvalsref) {
    typedef struct {
        str key;
        ModuleItem* item;
    } MICacheItem;
    static MICacheItem HOT_LOOKUP_CACHE[HOT_CACHE_SIZE];
    static MICacheItem COLD_LOOKUP_CACHE[COLD_CACHE_SIZE];
    static usize HOT_CACHE_LENGTH = 0;
    static usize COLD_CACHE_LENGTH = 0;
    static usize COLD_CACHE_INDEX = 0;
    str key = to_str_writer(s, {
        fprint_path(s, path);
        if (!path->absolute) {
            fprintf(s, "|");
            fprint_path(s, module->path);
        }
    });

    ModuleItem* mi = NULL;
    for (usize i = 0;i < HOT_CACHE_LENGTH;i++) {
        if (str_eq(HOT_LOOKUP_CACHE[i].key, key)) {
            MICacheItem mic = HOT_LOOKUP_CACHE[i];
            mi = mic.item;
            if (i > 0) { // improve location in cache
                HOT_LOOKUP_CACHE[i] = HOT_LOOKUP_CACHE[i - 1];
                HOT_LOOKUP_CACHE[i - 1] = mic;
            }
            ITEM_CACHE_HOT_HITS += 1;
            break;
        }
    }
    if (mi == NULL) {
        for (usize i = 0;i < COLD_CACHE_LENGTH;i++) {
            usize idx = (COLD_CACHE_LENGTH - 1 - i + COLD_CACHE_INDEX) % COLD_CACHE_LENGTH;
            if (str_eq(COLD_LOOKUP_CACHE[idx].key, key)) {
                MICacheItem mic = COLD_LOOKUP_CACHE[idx];
                mi = mic.item;
                // now swap this into the hot cache
                if (HOT_CACHE_LENGTH < HOT_CACHE_SIZE) {
                    HOT_LOOKUP_CACHE[HOT_CACHE_LENGTH++] = mic;
                } else {
                    // replace random:
                    usize replace_idx = rand() % HOT_CACHE_LENGTH;
                    COLD_LOOKUP_CACHE[idx] = HOT_LOOKUP_CACHE[replace_idx];
                    HOT_LOOKUP_CACHE[replace_idx] = mic;
                    // replace last:
                    // COLD_LOOKUP_CACHE[idx] = HOT_LOOKUP_CACHE[HOT_CACHE_LENGTH-1];
                    // HOT_LOOKUP_CACHE[HOT_CACHE_LENGTH-1] = mic;
                }
                ITEM_CACHE_COLD_HITS += 1;
                break;
            }
        }
    }

    if (mi == NULL) {
        mi = resolve_item_raw(program, options, module, path, kind, NULL);
        MICacheItem mic = (MICacheItem) { .key=key, .item = mi };
        ITEM_CACHE_MISSES += 1;
        // now place into cache
        if (HOT_CACHE_LENGTH < HOT_CACHE_SIZE) {
            HOT_LOOKUP_CACHE[HOT_CACHE_LENGTH++] = mic;
        } else if (COLD_CACHE_LENGTH < COLD_CACHE_SIZE) {
            COLD_LOOKUP_CACHE[COLD_CACHE_LENGTH++] = mic;
        } else {
            COLD_LOOKUP_CACHE[COLD_CACHE_INDEX] = mic;
            COLD_CACHE_INDEX = (COLD_CACHE_INDEX + 1) % COLD_CACHE_LENGTH;
        }
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

    GenericKeys* gkeys = NULL;
    switch (mi->type) {
        case MIT_FUNCTION: {
            FuncDef* func = mi->item;
            gkeys = func->generics;
        } break;
        case MIT_STRUCT: {
            TypeDef* type = mi->item;
            gkeys = type->generics;
        } break;
        case MIT_TRAIT: {
            TraitDef* trait = mi->item;
            gkeys = trait->keys;
        } break;
        case MIT_CONSTANT:
        case MIT_STATIC: {
            Global* g = mi->item;
            UNUSED(g);
        } break;
        default:
            unreachable();
    }

    str fullname = to_str_writer(stream, fprint_path(stream, path));
    GenericValues* gvals = *gvalsref;
    Span span = from_points(&path->elements.elements[path->elements.length-1]->span.left, &path->elements.elements[path->elements.length-1]->span.right);
    if ((gkeys == NULL || gkeys->generics.length == 0) && (gvals == NULL || gvals->generics.length == 0)) goto generic_end;
    if (gkeys == NULL && gvals != NULL) spanned_error("Unexpected generics", span, "Expected no generics for %s, got %llu", fullname, gvals->generics.length);
    if (gkeys != NULL && gvals == NULL) {
        gvals = malloc(sizeof(GenericValues));
        gvals->span = span;
        gvals->generics = list_new(TypeValueList);
        gvals->resolved = map_new();
        list_foreach(&gkeys->generics, i, GKey* generic, {
            UNUSED(generic);
            TypeValue* dummy = gen_typevalue("_", &span);
            list_append(&gvals->generics, dummy);
        });
        *gvalsref = gvals;
    }
    if (gkeys->generics.length != gvals->generics.length) spanned_error("Generics mismatch", gvals->span, "Expected %llu for %s, got %llu", gkeys->generics.length, fullname, gvals->generics.length);
    // We definitely have some generics here!
    list_foreach(&gkeys->generics, i, GKey* generic, {
        str key = generic->name->name;
        TypeValue* value = gvals->generics.elements[i];
        resolve_typevalue(program, options, module, value, func_generics, type_generics);
        map_put(gvals->resolved, key, value);
    });
    generic_end: {}
    if (kind == MIT_ANY) return mi;
    return mi->item;
}

static void tv_apply_traits(Program* program, CompilerOptions* options, TypeValue* tv) {
    if (tv->def->module == NULL) return; // is a generic key, those work differently
    if (options->verbosity >= 7) log("applying traits to %s", to_str_writer(s, fprint_full_typevalue(s, tv)));
    map_foreach(tv->def->module->package_trait_impl_map, str key, ImplBlock* impl, {
        UNUSED(key);
        if (impl->trait_ref == NULL) panic("Compiler error: This isn't even a trait???");
        if (impl->trait == NULL) { // during trait head resolution
            impl->trait = resolve_item_raw(program, options, impl->module, impl->trait_ref->name, MIT_TRAIT, NULL)->item;
            if (impl->type->def == NULL) {
                resolve_generic_keys(program, options, impl->module, impl->generics, NULL, NULL, NULL);
                resolve_typevalue(program, options, impl->module, impl->type, NULL, impl->generics);
            }
        }
        if (options->verbosity >= 8 && tv->def == impl->type->def) log("  > checking %s (%s) on %s", impl->trait->name->name, to_str_writer(s, fprint_full_typevalue(s, impl->type)), to_str_writer(s, fprint_typevalue(s, tv)));
        if (satisfies_impl_bounds(tv, impl->type)) { 
            if (options->verbosity >= 7) log("  < adding %s on %s", impl->trait->name->name, to_str_writer(s, fprint_typevalue(s, tv)));
            if (impl->trait == NULL) unreachable("Compiler error");
            map_put(tv->trait_impls, to_str_writer(s, fprintf(s, "%p", impl->trait)), impl);
        }
    });
    if (map_contains(tv->trait_impls, program->raii.copy_key) && map_contains(tv->trait_impls, program->raii.drop_key)) {
        ImplBlock* copy = map_get(tv->trait_impls, program->raii.copy_key);
        ImplBlock* drop = map_get(tv->trait_impls, program->raii.drop_key);
        spanned_error("Copy and Drop may not be implemented on the same type", tv->name->elements.elements[0]->span, 
            "\n::core::copy::Copy implemented @ %s\n::core::drop::Drop implemented @ %s\nFull type: %s",
            to_str_writer(s, fprint_span(s, &copy->type->name->elements.elements[0]->span)), 
            to_str_writer(s, fprint_span(s, &drop->type->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_full_typevalue(s, tv)));
    }
    if (options->verbosity >= 7) log("applied traits to %s", to_str_writer(s, fprint_full_typevalue(s, tv)));
}

void resolve_typevalue(Program* program, CompilerOptions* options, Module* module, TypeValue* tval, GenericKeys* func_generics, GenericKeys* type_generics) {
    if (tval->def != NULL) return;
    if (options->verbosity >= 6) log("resolving %s %s", to_str_writer(s, fprint_typevalue(s, tval)), to_str_writer(s, fprint_span(s, &tval->name->elements.elements[0]->span)));
    // might be a generic? >`OvO´<
    if (!tval->name->absolute && tval->name->elements.length == 1) {
        str key = tval->name->elements.elements[0]->name;
        if (str_eq(key, "_")) {
            return;
        }
        TypeDef* func_gen_t = NULL;
        TypeDef* type_gen_t = NULL;
        if (func_generics != NULL) func_gen_t = map_get(func_generics->resolved, key);
        if (type_generics != NULL) type_gen_t = map_get(type_generics->resolved, key);
        if (func_gen_t != NULL && type_gen_t != NULL) spanned_error("Multiple generic candidates", tval->name->elements.elements[0]->span, "Generic %s could belong to %s or %s", 
                    tval->name->elements.elements[0]->name,
                    to_str_writer(stream, fprint_span(stream, &func_gen_t->name->span)),
                    to_str_writer(stream, fprint_span(stream, &type_gen_t->name->span)));
        if ((func_gen_t != NULL || type_gen_t != NULL) && tval->generics != NULL && tval->generics->generics.length > 0) spanned_error("Compiler error: Generic generic", tval->generics->span, "Generic parameter should not have generic arguments");
        if (func_gen_t != NULL) {
            tval->def = func_gen_t;
            tval->ctx = func_generics;
            return;
        }
        if (type_gen_t != NULL) {
            tval->def = type_gen_t;
            tval->ctx = type_generics;
            return;
        }
    }
    // it is an actual type and not a generic parameter
    TypeDef* td = resolve_item(program, options, module, tval->name, MIT_STRUCT, func_generics, type_generics, &tval->generics);
    tval->def = td;
    if (tval->generics != NULL) {
        list_foreach(&tval->generics->generics, i, TypeValue* tv, {
            resolve_typevalue(program, options, module, tv, func_generics, type_generics);
        });
    }
    tv_apply_traits(program, options, tval);
}

void assert_types_equal(Program* program, CompilerOptions* options, Module* module, TypeValue* tv1, TypeValue* tv2, Span span, GenericKeys* func_generics, GenericKeys* type_generics) {
    if (tv1->def == NULL) resolve_typevalue(program, options,  module, tv1, func_generics, type_generics);
    if (tv2->def == NULL) resolve_typevalue(program, options, module, tv2, func_generics, type_generics);
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
            assert_types_equal(program, options, module, tv1->generics->generics.elements[i], tv2->generics->generics.elements[i], span, func_generics, type_generics);
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

static void find_variable(Program* program, CompilerOptions* options, Module* module, VarList* vars, Variable* var, GenericKeys* func_generics, GenericKeys* type_generics) {
    Path* path = var->path;
    Identifier* name = path->elements.elements[path->elements.length-1];
    if (!path->absolute && path->elements.length == 1) {
        for (int i = vars->length-1;i >= 0;i--) {
            if (str_eq(vars->elements[i].var->name, name->name)) {
                var->box = vars->elements[i].var;
                var->global_ref = NULL;
                if (var->values != NULL) spanned_error("Generic variable?", var->path->elements.elements[0]->span, "Variable values are not supposed to be generic");
                return;
            }
        }
    }
    VarBox* box = malloc(sizeof(VarBox));
    box->id = ~0;
    box->name = NULL;
    box->values = var->values;
    var->box = box;
    if (var->method_name != NULL) {
        if (var->method_values == NULL) {
            var->method_values = malloc(sizeof(GenericValues));
            var->method_values->generics = list_new(TypeValueList);
            var->method_values->resolved = map_new();
            var->method_values->span = var->method_name->span;
        }
        TypeValue* tv = malloc(sizeof(TypeValue));
        tv->name = path;
        tv->generics = var->values;
        tv->ctx = NULL;
        tv->def = NULL;
        tv->trait_impls = map_new();
        resolve_typevalue(program, options, module, tv, func_generics, type_generics);
        FuncDef* method = resolve_method_instance(program, options, tv, var->method_name, true);
        if (!method->body_resolved) resolve_funcdef_body(program, options, method);
        if (var->method_values->generics.length != method->generics->generics.length) spanned_error("Mismatch generic count", var->method_values->span, "Expected %lld generics, got %lld", method->generics->generics.length, var->method_values->generics.length);
        for (usize i = 0; i < var->method_values->generics.length;i++) {
            str key = method->generics->generics.elements[i]->name->name;
            TypeValue* generic_value = var->method_values->generics.elements[i];
            resolve_typevalue(program, options, method->module, generic_value, method->generics, tv->def->generics);
            map_put(var->method_values->resolved, key, generic_value);
        }
        TypeValue* funcptr = gen_typevalue("std::function_ptr<T>", &name->span);
        resolve_typevalue(program, options, module, method->return_type, NULL, NULL);
        TypeValue* ret = replace_generic(program, options, method->return_type, var->values, var->method_values, method->trait, tv);
        funcptr->generics->generics.elements[0] = ret;
        box->resolved = malloc(sizeof(TVBox));
        box->resolved->type = funcptr;
        box->mi = malloc(sizeof(ModuleItem));
        box->mi->item = method;
        box->ty = tv;
        box->is_copy = true;
        if (tv->def->module == NULL) {
            bool found = false;
            list_find_map(&tv->def->key->bounds, i, TraitBound** bound_ref, (*bound_ref)->resolved == method->trait, {
                found = true;
                break;
            });
            if (!found) panic("Compiler error: trait not found");
        } else {
            validate_item(program, options, var->values, var->method_values, method->type_generics, method->generics, method->generics);
            validate_item(program, options, var->values, var->method_values, method->type_generics, method->generics, method->type_generics);
        }
        return;
    }
    ModuleItem* item = resolve_item(program, options, module, path, MIT_ANY, func_generics, type_generics, &var->values);
    box->mi = item;
    switch (item->type) {
        case MIT_FUNCTION: {
            FuncDef* func = item->item;
            if (!func->body_resolved) resolve_funcdef_body(program, options, func);
            if (func->generics != NULL) {
                if (var->values == NULL) spanned_error("Generic function", var->path->elements.elements[0]->span, "this function is generic. please provide generic args.");
                if (func->generics->generics.length != var->values->generics.length) spanned_error("Mismatch generic count", var->path->elements.elements[0]->span, "title says it all, todo better errors in this entire area...");
            } else if (var->values != NULL) spanned_error("Ungeneric function", var->path->elements.elements[0]->span, "this function is not generic");
            TypeValue* tv = gen_typevalue("std::function_ptr<T>", &name->span);
            resolve_typevalue(program, options, module, func->return_type, NULL, NULL);
            TypeValue* ret = replace_generic(program, options, func->return_type, NULL, var->values, NULL, NULL);
            tv->generics->generics.elements[0] = ret;
            box->resolved = malloc(sizeof(TVBox));
            box->resolved->type = tv;
            validate_item(program, options, NULL, var->values, NULL, func->generics, func->generics);
            box->is_copy = true;
        } break;
        case MIT_STATIC: {
            if (var->values != NULL) spanned_error("Generic static?", var->path->elements.elements[0]->span, "Static values are not supposed to be generic");
            Global* s = item->item;
            box->resolved = malloc(sizeof(TVBox));
            box->resolved->type = s->type;
            var->global_ref = s;
            if (map_contains(box->resolved->type->trait_impls, program->raii.copy_key)) {
                box->is_copy = true;
            } else {
                box->is_copy = false;
            }
        } break;
        case MIT_CONSTANT: {
            if (var->values != NULL) spanned_error("Generic const?", var->path->elements.elements[0]->span, "Constants are not supposed to be generic");
            Global* s = item->item;
            box->resolved = malloc(sizeof(TVBox));
            box->resolved->type = s->type;
            var->global_ref = s;
            if (map_contains(box->resolved->type->trait_impls, program->raii.copy_key)) {
                box->is_copy = true;
            } else {
                box->is_copy = false;
            }
        } break;
        case MIT_STRUCT:
            spanned_error("Expected variable", name->span, "Expected variable or function ref, got struct");
            break;
        case MIT_TRAIT:
            spanned_error("Expected variable", name->span, "Expected variable or function ref, got trait");
            break;
        case MIT_MODULE:
            spanned_error("Expected variable", name->span, "Expected variable or function ref, got module");
            break;
        case MIT_ANY:
            unreachable();
    }
}

static VarBox* register_variable(Program* program, CompilerOptions* options, VarList* vars, Variable* var, TVBox* box) {
    Identifier* name = var->path->elements.elements[var->path->elements.length-1];
    if (var->path->absolute || var->path->elements.length != 1) spanned_error("Variable has too much path", name->span, "This should look more like a single variable and should have been caught earlier, that's a compiler bug.");
    VarBox* v = malloc(sizeof(VarBox));
    v->name = name->name;
    v->id = vars->counter;
    vars->counter += 1;
    v->resolved = box;
    v->mi = NULL;
    v->values = NULL;
    StackVar sv = { .var=v };
    if (map_contains(v->resolved->type->trait_impls, program->raii.copy_key)) {
        v->is_copy = true;
        sv.state = VS_COPY;
    } else {
        v->is_copy = false;
        sv.state = VS_NONCOPY;
    }
    var->box = v;
    v->stack_index = vars->length;
    list_append(vars, sv);
    return v;
}

TypeValue* replace_generic(Program* program, CompilerOptions* options, TypeValue* tv, GenericValues* type_ctx, GenericValues* func_ctx, TraitDef* trait, TypeValue* trait_called) {
    if (type_ctx == NULL && func_ctx == NULL) return tv;
    if (tv->generics != NULL && tv->generics->generics.length > 0) {
        TypeValue* clone = malloc(sizeof(TypeValue));
        clone->def = tv->def;
        clone->name = tv->name;
        clone->ctx = tv->ctx;
        clone->trait_impls = map_new();
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
            val = replace_generic(program, options, val, type_ctx, func_ctx, trait, trait_called);
            list_append(&clone->generics->generics, val);
            map_put(clone->generics->resolved, real_key, val);
        });
        map_free(rev);
        tv_apply_traits(program, options, clone);
        return clone;
    }
    if (tv->name->absolute || tv->name->elements.length != 1) return tv;
    str name = tv->name->elements.elements[0]->name;
    TypeValue* type_candidate = NULL;
    if (type_ctx != NULL) type_candidate = map_get(type_ctx->resolved, name);
    TypeValue* func_candidate = NULL;
    if (func_ctx != NULL) func_candidate = map_get(func_ctx->resolved, name);
    TypeValue* trait_candidate = NULL;
    if (trait != NULL && str_eq(trait->self_key->name->name, name)) trait_candidate = trait_called; 
    if (type_candidate != NULL && func_candidate != NULL) {
        spanned_error("Multiple generic candidates (Ty/Fn)", tv->name->elements.elements[0]->span, "Generic `%s` could be replaced by %s @ %s or %s @ %s", name,
            to_str_writer(s, fprint_typevalue(s, type_candidate)), to_str_writer(s, fprint_span(s, &type_candidate->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_typevalue(s, func_candidate)), to_str_writer(s, fprint_span(s, &func_candidate->name->elements.elements[0]->span)));
    }
    if (false && type_candidate != NULL && trait_candidate != NULL) {
        spanned_error("Multiple generic candidates (Ty/Tr)", tv->name->elements.elements[0]->span, "Generic `%s` could be replaced by %s @ %s or %s @ %s", name,
            to_str_writer(s, fprint_typevalue(s, type_candidate)), to_str_writer(s, fprint_span(s, &type_candidate->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_typevalue(s, trait_candidate)), to_str_writer(s, fprint_span(s, &trait_candidate->name->elements.elements[0]->span)));
    }
    if (false && trait_candidate != NULL && func_candidate != NULL) {
        spanned_error("Multiple generic candidates (Tr/Fn)", tv->name->elements.elements[0]->span, "Generic `%s` could be replaced by %s @ %s or %s @ %s", name,
            to_str_writer(s, fprint_typevalue(s, trait_candidate)), to_str_writer(s, fprint_span(s, &trait_candidate->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_typevalue(s, func_candidate)), to_str_writer(s, fprint_span(s, &func_candidate->name->elements.elements[0]->span)));
    }
    if (type_candidate != NULL) return type_candidate;
    if (func_candidate != NULL) return func_candidate;
    if (trait_candidate != NULL) return trait_candidate;
    return tv;
}

void patch_tvs(TypeValue** tv1ref, TypeValue** tv2ref) {
    TypeValue* tv1 = *tv1ref;
    TypeValue* tv2 = *tv2ref;
    // log("patching `%s` and `%s`", to_str_writer(s, fprint_typevalue(s, tv1)), to_str_writer(s, fprint_typevalue(s, tv2)));
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
    if (template->generics == NULL) {
        // is generic param?
        if (template->def->module == NULL) {
            str name = template->name->elements.elements[0]->name;
            list_foreach(&keys->generics, i, GKey* key, {
                if (str_eq(key->name->name, name)) {
                    gvals->generics.elements[i] = match;
                    map_put(gvals->resolved, name, match);
                    break;
                }
            });
        }
        return;
    }
    if ((template->generics == NULL && match->generics != NULL) || (template->generics != NULL && match->generics == NULL)) {
        spanned_error("Type structure mismatch", match->name->elements.elements[0]->span, "Type %s @ %s does not structurally match its tempate %s @ %s (generic/ungeneric)",
            to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &match->name->elements.elements[0]->span)),
            to_str_writer(s, fprint_typevalue(s, template)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)));
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
    list_foreach(&keys->generics, i, GKey* key, {
        UNUSED(key);
        list_append(&gvals->generics, NULL); // insert dummy for now
    });
    r_collect_generics(template, match, keys, gvals);
    list_foreach(&keys->generics, i, GKey* key, {
        UNUSED(key);
        if (gvals->generics.elements[i] == NULL) {
            spanned_error("Unbound generic", key->name->span, "Generic `%s` was not bound by %s @ %s and as such could not be filled by %s @ %s", 
                key->name->name, 
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
            TypeValue* func_candidate = NULL;
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
    spanned_error("Generic mismatch", value->name->elements.elements[0]->span, "%s @ %s, %s @ %s and %s @ %s do not match in generics", 
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

void fill_tvbox(Program *program, CompilerOptions* options, Module *module, Span span, GenericKeys* func_generics, GenericKeys* type_generics, TVBox* box, TypeValue* value) {
    if (value->def == NULL) resolve_typevalue(program, options, module, value, func_generics, type_generics);
    if (box->type != NULL) {
        patch_tvs(&box->type, &value);
        assert_types_equal(program, options, module, box->type, value, span, func_generics, type_generics);
    } else {
        box->type = value;
    }
}

static bool satisfies_impl_bounds(TypeValue* tv, TypeValue* impl_tv) {
    // if (tv->def == NULL || tv->def == impl_tv->def) log("  %s | %s", to_str_writer(s, fprint_full_typevalue(s, tv)), to_str_writer(s, fprint_full_typevalue(s, impl_tv)));
    if (impl_tv->def == NULL) panic("Compiler error: unresolved type (impl)");
    if (impl_tv->def->module != NULL && tv->def->module == NULL) return false; // concrete given, we have generic -> that doesn't work!
    if (impl_tv->def->traits.length == 0) {
        if (impl_tv->def->module == NULL) return true;
        if (tv->def == NULL && tv->name->elements.length == 1 && str_eq(tv->name->elements.elements[0]->name, "_")) return true;
    }
    if (tv->def == NULL && tv->name->elements.length == 1 && str_eq(tv->name->elements.elements[0]->name, "_")) return false;
    if (tv->def == NULL) panic("Compiler error: unresolved type");
    if (impl_tv->def->module != NULL && tv->def->module != NULL && tv->def != impl_tv->def) return false; // concrete types and a mismatch!
    if (impl_tv->def->module == NULL) { // check trait bounds if this is a generic
        list_foreach(&impl_tv->def->traits, i, TraitDef* trait, {
            if (tv->def->module == NULL) {
                if (!list_contains(&tv->def->traits, j, TraitDef* t, t == trait)) return false;
            } else {
                if (!map_contains(tv->trait_impls, to_str_writer(s, fprintf(s, "%p", trait)))) return false;
            } 
        });
    }
    if (tv->def->module != NULL && impl_tv->def->generics != NULL) {
        if ((tv->generics == NULL || tv->generics->generics.length == 0) && (impl_tv->generics == NULL || impl_tv->generics->generics.length == 0)) return true; // no generics
        if (tv->generics->generics.length != impl_tv->generics->generics.length) unreachable("Compiler error: generic mismatch");
        for(usize i = 0; i < tv->generics->generics.length; i++) {
            if (!satisfies_impl_bounds(tv->generics->generics.elements[i], impl_tv->generics->generics.elements[i])) return false;
        }
    }
    return true;
}

FuncDef* resolve_method_instance(Program* program, CompilerOptions* options, TypeValue* tv, Identifier* name, bool do_error) {
    if (options->verbosity >= 6) log("resolving %s::%s %s", to_str_writer(s, fprint_typevalue(s, tv)), name->name, to_str_writer(s, fprint_span(s, &name->span)));
    Module* mod = tv->def->module;
    if (mod == NULL) { // generic type - search through trait bounds
        FuncDef* chosen = NULL;
        list_foreach(&tv->def->traits, i, TraitDef* t, {
            ModuleItem* mi = map_get(t->methods, name->name);
            if (mi == NULL) continue;
            FuncDef* method = mi->item;
            if (chosen != NULL) {
                if (!do_error) return NULL;
                spanned_error("Multiple method candiates", tv->name->elements.elements[0]->span, "Multiple candiates found for %s::%s\n#1: %s::%s @ %s\n#2: %s::%s @ %s", 
                to_str_writer(s, fprint_typevalue(s, tv)), name->name,
                to_str_writer(s, fprint_typevalue(s, method->impl_type)), method->name->name, to_str_writer(s, fprint_span(s, &method->name->span)),
                to_str_writer(s, fprint_typevalue(s, chosen->impl_type)), chosen->name->name, to_str_writer(s, fprint_span(s, &chosen->name->span)));
            }
            chosen = method;
        });
        if (chosen != NULL) {
            return chosen;
        }
        if (!do_error) return NULL;
        if (tv->def->traits.length > 0) spanned_error("Invalid type", name->span, "Cannot call method `%s` on generic type %s (%s). No trait bounds specify this method.", name->name, to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_type(s, tv->def)));
        else spanned_error("Invalid type", name->span, "Cannot call method `%s` on generic type %s. Try adding a trait bound supplying this method.\nFor example if trait `Foo` supplies method `%s`, you can add the bound in your generic parameter list:\n\t`<T: Foo, ...>`", name->name, to_str_writer(s, fprint_typevalue(s, tv)), name->name);
    }
    FuncList* list = map_get(mod->package_method_map, name->name);
    Module* base = tv->def->module;
    while (base->parent != NULL) base = base->parent;
    if (list == NULL || list->length == 0) spanned_error("No such method", name->span, "No such method `%s` defined on any type in package `%s` (on %s).", name->name, base->name->name,  to_str_writer(s, fprint_typevalue(s, tv)));
    FuncDef* chosen = NULL;
    list_foreach(list, i, FuncDef* func, {
        if (satisfies_impl_bounds(tv, func->impl_type)) {
            if (chosen != NULL) spanned_error("Multiple method candiates", tv->name->elements.elements[0]->span, "Multiple candiates found for %s::%s\n#1: %s::%s @ %s\n#2: %s::%s @ %s", 
                to_str_writer(s, fprint_typevalue(s, tv)), name->name,
                to_str_writer(s, fprint_typevalue(s, func->impl_type)), func->name->name, to_str_writer(s, fprint_span(s, &func->name->span)),
                to_str_writer(s, fprint_typevalue(s, chosen->impl_type)), chosen->name->name, to_str_writer(s, fprint_span(s, &chosen->name->span)));
            chosen = func;
        }
    });
    if (chosen == NULL) {
        if (!do_error) return NULL;
        spanned_error("No such method", name->span, "No such method `%s`\non type %s @ %s\naka %s\ndefined in package `%s`", 
        name->name, to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &tv->def->name->span)), to_str_writer(s, fprint_full_typevalue(s, tv)), base->name->name);
    }
    return chosen;
}

void finish_tvbox(Program* program, CompilerOptions* options, TVBox* box, FuncDef* ctx) {
    validate_item(program, options, box->type->generics, NULL, box->type->def->generics, NULL, box->type->def->generics);
}

void resolve_expr(Program* program, CompilerOptions* options, FuncDef* func, Expression* expr, VarList* vars, TVBox* t_return, bool asref) {
    Expression* last = CURRENT_EXPR;
    CURRENT_EXPR = expr;
    if (options->verbosity >= 8) log("resolving %s in %s @ %s", ExprType__NAMES[expr->type], func->name->name, to_str_writer(s, fprint_span(s, &expr->span)));
    expr->resolved = t_return;
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            TVBox* op_arg_box;
            if (str_eq(op->op, ">") || str_eq(op->op, "<") || str_eq(op->op, ">=") || str_eq(op->op, "<=") || str_eq(op->op, "!=") || str_eq(op->op, "==")) {
                op_arg_box = new_tvbox();
                TypeValue* bool_ty = gen_typevalue("::core::types::bool", &op->op_span);
                resolve_typevalue(program, options, func->module, bool_ty, func->generics, func->type_generics);
                fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, bool_ty);
            } else {
                op_arg_box = t_return;
            }
            resolve_expr(program, options, func, op->lhs, vars, op_arg_box, false);
            resolve_expr(program, options, func, op->rhs, vars, op_arg_box, false);
        } break;
        case EXPR_BIN_OP_ASSIGN: {
            BinOp* op = expr->expr;
            TVBox* op_arg_box = new_tvbox();
            resolve_expr(program, options, func, op->lhs, vars, op_arg_box, true);
            resolve_expr(program, options, func, op->rhs, vars, op_arg_box, false);
            TypeValue* unit_ty = gen_typevalue("::core::types::unit", &op->op_span);
            resolve_typevalue(program, options, func->module, unit_ty, func->generics, func->type_generics);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, unit_ty);
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncDef* fd = resolve_item(program, options, func->module, fc->name, MIT_FUNCTION, func->generics, func->type_generics, &fc->generics);
            if (!fd->body_resolved) resolve_funcdef_body(program, options, fd);
            // preresolve return
            TypeValue* pre_ret = replace_generic(program, options, fd->return_type, NULL, fc->generics, NULL, NULL);
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
                    TypeValue* arg_tv = replace_generic(program, options, fd->args.elements[i]->type, NULL, fc->generics, NULL, NULL);
                    argbox->type = arg_tv;
                    resolve_expr(program, options, func, arg, vars, argbox, false);
                    patch_generics(replace_generic(program, options, fd->args.elements[i]->type, NULL, fc->generics, NULL, NULL), fd->args.elements[i]->type, argbox->type, NULL, fc->generics);
                    finish_tvbox(program, options, argbox, func);
                } else { // variadic arg
                    TVBox* argbox = new_tvbox();
                    resolve_expr(program, options, func, arg, vars, argbox, false);
                    finish_tvbox(program, options, argbox, func);
                }
            });

            TypeValue* ret = replace_generic(program, options, fd->return_type, NULL, fc->generics, NULL, NULL);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, ret);
            validate_item(program, options, NULL, fc->generics, NULL, fd->generics, fd->generics);
        } break;
        case EXPR_METHOD_CALL: {
            MethodCall* call = expr->expr;
            TVBox* objectbox = new_tvbox();
            resolve_expr(program, options, func, call->object, vars, objectbox, true);

            TypeValue* ptr = gen_typevalue("::core::types::ptr<_>", &expr->span);
            resolve_typevalue(program, options, func->module, ptr, NULL, NULL);

            TypeValue* method_type = call->object->resolved->type;
            bool deref_to_call = false;
            if (method_type->def == ptr->def) {
                method_type = method_type->generics->generics.elements[0];
                deref_to_call = true;
            }
            FuncDef* method = resolve_method_instance(program, options, method_type, call->name, true);
            if (!method->body_resolved) resolve_funcdef_body(program, options, method);
            call->def = method;
            if (call->generics != NULL) {
                if (call->generics->generics.length != method->generics->generics.length) spanned_error("Generic count mismatch", expr->span, "Expected %lld generics, got %lld", call->generics->generics.length, method->generics->generics.length);
                for (usize i = 0; i < call->generics->generics.length;i++) {
                    str key = method->generics->generics.elements[i]->name->name;
                    TypeValue* generic_value = call->generics->generics.elements[i];
                    resolve_typevalue(program, options, func->module, generic_value, func->generics, func->type_generics);
                    map_put(call->generics->resolved, key, generic_value);
                }
            } else if(method->generics != NULL) {
                call->generics = malloc(sizeof(GenericValues));
                call->generics->span = expr->span;
                call->generics->generics = list_new(TypeValueList);
                call->generics->resolved = map_new();
                list_foreach(&method->generics->generics, i, GKey* key, {
                    TypeValue* dummy = gen_typevalue("_", &expr->span);
                    list_append(&call->generics->generics, dummy);
                    map_put(call->generics->resolved, key->name->name, dummy);
                });
            }

            if (call->def->args.length == 0) spanned_error("Method call on zero arg method", expr->span, "Expected at least one argument (the calling object %s), but the method defines zero.\nTry calling statically as %s::<>::%s()", 
                to_str_writer(s, fprint_typevalue(s, call->object->resolved->type)), to_str_writer(s, fprint_typevalue(s, call->object->resolved->type)), method->name->name);
            if (call->def->is_variadic) {
                if (call->def->args.length > call->arguments.length + 1) spanned_error("Too few args for variadic method", expr->span, "expected at least %llu args, got %llu", call->def->args.length - 1, call->arguments.length);
            } else {
                if (call->def->args.length != call->arguments.length + 1) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", call->def->args.length - 1, call->arguments.length);
            }

            TypeValue* arg_tv = call->def->args.elements[0]->type;
            TypeValue* called_type = NULL;

            if (ptr->def == arg_tv->def) { // arg is a ptr
                if (!deref_to_call) { // we dont have a ptr yet
                    Expression* inner = call->object;
                    call->object = malloc(sizeof(Expression));
                    call->object->type = EXPR_TAKEREF;
                    call->object->span = inner->span;
                    call->object->expr = inner;

                    objectbox = new_tvbox();
                    resolve_expr(program, options, func, call->object, vars, objectbox, false);
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
                    resolve_expr(program, options, func, call->object, vars, objectbox, false);
                }
                called_type = call->object->resolved->type;
            }
            call->tv = called_type;
            GenericValues* type_call_vals = collect_generics(method->impl_type, called_type, method->type_generics);
            call->impl_vals = type_call_vals;

            patch_generics(replace_generic(program, options, call->def->args.elements[0]->type, type_call_vals, call->generics, method->trait, called_type), call->def->args.elements[0]->type, objectbox->type, NULL, call->generics);
            finish_tvbox(program, options, objectbox, func);
            
            // preresolve return
            TypeValue* pre_ret = replace_generic(program, options, call->def->return_type, type_call_vals, call->generics, method->trait, called_type);
            if (t_return->type != NULL) patch_generics(pre_ret, call->def->return_type, t_return->type, type_call_vals, call->generics);

            // resolving args
            list_foreach(&call->arguments, i, Expression* arg, {
                if (i + 1 < call->def->args.length) {
                    TVBox* argbox = new_tvbox();
                    TypeValue* arg_tv = replace_generic(program, options, call->def->args.elements[i+1]->type, type_call_vals, call->generics, method->trait, called_type);
                    argbox->type = arg_tv;
                    resolve_expr(program, options, func, arg, vars, argbox, false);
                    patch_generics(replace_generic(program, options, call->def->args.elements[i+1]->type, type_call_vals, call->generics, method->trait, called_type), call->def->args.elements[i+1]->type, argbox->type, type_call_vals, call->generics);
                    finish_tvbox(program, options, argbox, func);
                } else { // variadic arg
                    TVBox* argbox = new_tvbox();
                    resolve_expr(program, options, func, arg, vars, argbox, false);
                    finish_tvbox(program, options, argbox, func);
                }
            });

            list_foreach(&call->impl_vals->generics, i, TypeValue* gtv, {
                if (gtv->def == NULL) resolve_typevalue(program, options, func->module, gtv, func->generics, func->type_generics);
            });

            TypeValue* ret = replace_generic(program, options, call->def->return_type, type_call_vals, call->generics, method->trait, called_type);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, ret);

            if (called_type->def->module == NULL) {
                bool found = false;
                list_find_map(&called_type->def->key->bounds, i, TraitBound** bound_ref, (*bound_ref)->resolved == method->trait, {
                    found = true;
                    break;
                });
                if (!found) panic("Compiler error: trait not found");
            } else {
                validate_item(program, options, type_call_vals, call->generics, call->def->type_generics, call->def->generics, call->def->generics);    
                validate_item(program, options, type_call_vals, call->generics, call->def->type_generics, call->def->generics, call->def->type_generics);    
            }
        } break;
        case EXPR_STATIC_METHOD_CALL: {
            StaticMethodCall* call = expr->expr;
            resolve_typevalue(program, options, func->module, call->tv, func->generics, func->type_generics);
            FuncDef* method = resolve_method_instance(program, options, call->tv, call->name, true);
            if (!method->body_resolved) resolve_funcdef_body(program, options, method);
            call->def = method;
            if (call->generics != NULL) {
                if (call->generics->generics.length != method->generics->generics.length) spanned_error("Generic count mismatch", expr->span, "Expected %lld generics, got %lld", call->generics->generics.length, method->generics->generics.length);
                for (usize i = 0; i < call->generics->generics.length;i++) {
                    str key = method->generics->generics.elements[i]->name->name;
                    TypeValue* generic_value = call->generics->generics.elements[i];
                    resolve_typevalue(program, options, func->module, generic_value, func->generics, func->type_generics);
                    map_put(call->generics->resolved, key, generic_value);
                }
            } else if(method->generics != NULL) {
                call->generics = malloc(sizeof(GenericValues));
                call->generics->span = expr->span;
                call->generics->generics = list_new(TypeValueList);
                call->generics->resolved = map_new();
                list_foreach(&method->generics->generics, i, GKey* key, {
                    TypeValue* dummy = gen_typevalue("_", &expr->span);
                    list_append(&call->generics->generics, dummy);
                    map_put(call->generics->resolved, key->name->name, dummy);
                });
            }
            GenericValues* type_call_vals = collect_generics(method->impl_type, call->tv, method->type_generics);
            call->impl_vals = type_call_vals;

            // preresolve return
            TypeValue* pre_ret = replace_generic(program, options, call->def->return_type, type_call_vals, call->generics, method->trait, call->tv);
            if (t_return->type != NULL) patch_generics(pre_ret, call->def->return_type, t_return->type, type_call_vals, call->generics);

            if (call->def->is_variadic) {
                if (call->def->args.length > call->arguments.length) spanned_error("Too few args for variadic method", expr->span, "expected at least %llu args, got %llu", call->def->args.length, call->arguments.length);
            } else {
                if (call->def->args.length != call->arguments.length) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", call->def->args.length, call->arguments.length);
            }

            // resolving args
            list_foreach(&call->arguments, i, Expression* arg, {
                if (i < call->def->args.length) {
                    TVBox* argbox = new_tvbox();
                    TypeValue* arg_tv = replace_generic(program, options, call->def->args.elements[i]->type, type_call_vals, call->generics, method->trait, call->tv);
                    argbox->type = arg_tv;
                    resolve_expr(program, options, func, arg, vars, argbox, false);
                    patch_generics(replace_generic(program, options, call->def->args.elements[i]->type, type_call_vals, call->generics, method->trait, call->tv), call->def->args.elements[i]->type, argbox->type, type_call_vals, call->generics);
                    finish_tvbox(program, options, argbox, func);
                } else { // variadic arg
                    TVBox* argbox = new_tvbox();
                    resolve_expr(program, options, func, arg, vars, argbox, false);
                    finish_tvbox(program, options, argbox, func);
                }
            });

            list_foreach(&call->impl_vals->generics, i, TypeValue* gtv, {
                if (gtv->def == NULL) resolve_typevalue(program, options, func->module, gtv, func->generics, func->type_generics);
            });

            TypeValue* ret = replace_generic(program, options, call->def->return_type, type_call_vals, call->generics, method->trait, call->tv);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, ret);
            
            if (call->tv->def->module == NULL) {
                bool found = false;
                list_find_map(&call->tv->def->key->bounds, i, TraitBound** bound_ref, (*bound_ref)->resolved == method->trait, {
                    found = true;
                    break;
                });
                if (!found) panic("Compiler error: trait not found");
            } else {
                validate_item(program, options, type_call_vals, call->generics, call->def->type_generics, call->def->generics, call->def->generics);  
                validate_item(program, options, type_call_vals, call->generics, call->def->type_generics, call->def->generics, call->def->type_generics);  
            }
        } break;
        case EXPR_DYN_RAW_CALL: {
            DynRawCall* call = expr->expr;

            TVBox* dyncall = new_tvbox();
            dyncall->type = gen_typevalue("::core::types::function_ptr<_>", &expr->span);
            if (t_return->type != NULL) dyncall->type->generics->generics.elements[0] = t_return->type;
            resolve_typevalue(program, options, func->module, dyncall->type, NULL, NULL);
            resolve_expr(program, options, func, call->callee, vars, dyncall, true);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, dyncall->type->generics->generics.elements[0]);
            // resolving args, no typehints here, unsafe yada yada
            list_foreach(&call->args, i, Expression* arg, {
                TVBox* argbox = new_tvbox();
                resolve_expr(program, options, func, arg, vars, argbox, false);
                finish_tvbox(program, options, argbox, func);
            });
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING: {
                    t_return->type = gen_typevalue("::core::types::c_str", &expr->span);
                    resolve_typevalue(program, options, func->module, t_return->type, func->generics, func->type_generics);
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
                        tv = gen_typevalue(to_str_writer(stream, fprintf(stream, "::core::types::%s", ty)), &expr->span);
                    }
                    fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, tv);
                } break; 
                default:
                    unreachable("invalid literal type %s", TokenType__NAMES[lit->type]);
            }
        } break;
        case EXPR_BLOCK: {
            Block* block = expr->expr;
            resolve_block(program, options, func, block, vars, t_return);
        } break;
        case EXPR_VARIABLE: {
            Variable* var = expr->expr;
            find_variable(program, options, func->module, vars, var, func->generics, func->type_generics);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, var->box->resolved->type);
            if (!asref) { // if it's a ref we do not need to check
                if(!var->box->is_copy) {
                    if (var->global_ref != NULL) {
                        spanned_error("Moving global", var->global_ref->name->span, "Cannot move global variable of type %s @ %s.\nTry implementing core::copy::Copy or taking a reference instead of moving", to_str_writer(s, fprint_typevalue(s, var->box->resolved->type)), to_str_writer(s, fprint_span(s, &var->box->resolved->type->name->elements.elements[0]->span)));
                    } else {
                        StackVar* sv = &vars->elements[var->box->stack_index];
                        switch (sv->state) {
                        case VS_COPY: { // pass - can be copied
                    } break;
                        case VS_NONCOPY: {
                            // declared outside while loop
                            if (var->box->stack_index < vars->loop_index) {
                                if (vars->return_flag || vars->break_flag) {
                                    if (vars->return_flag && vars->return_move == NULL) vars->return_move = var; 
                                    if (vars->break_flag && vars->break_move == NULL) vars->break_move = var; 
                                } else {
                                    spanned_error("Moving variable in loop", var->path->elements.elements[0]->span, "Cannot move variable while inside loop as it might be moved again during the next iteration");
                                }
                            }
                            sv->state = VS_MOVED; // got moved
                            sv->moved_at = expr->span;
                        } break;
                        case VS_MOVED: {
                            spanned_error("Accessing moved variable", sv->moved_at, "This variable of type %s @ %s was alread moved here.\nTry implementing core::copy::Copy or taking a reference instead of moving", to_str_writer(s, fprint_typevalue(s, var->box->resolved->type)), to_str_writer(s, fprint_span(s, &var->box->resolved->type->name->elements.elements[0]->span)));
                        } break;
                        }
                    }
                }
            }
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;
            TVBox* box = new_tvbox();

            if (let->type != NULL) {
                resolve_typevalue(program, options, func->module, let->type, func->generics, func->type_generics);
                fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, box, let->type);
            }
            resolve_expr(program, options, func, let->value, vars, box, false);
            patch_tvs(&let->type, &box->type);

            register_variable(program, options, vars, let->var, box);

            TypeValue* tv = gen_typevalue("::core::types::unit", &expr->span);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, tv);
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            TVBox* assign_t = new_tvbox();
            resolve_expr(program, options, func, assign->asignee, vars, assign_t, true);
            resolve_expr(program, options, func, assign->value, vars, assign_t, false);

            TypeValue* tv = gen_typevalue("::core::types::unit", &expr->span);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, tv);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;

            TypeValue* bool_ty = gen_typevalue("::core::types::bool", &cond->cond->span);
            resolve_typevalue(program, options, func->module, bool_ty, func->generics, func->type_generics);
            TVBox* cond_ty = new_tvbox();
            cond_ty->type = bool_ty;
            resolve_expr(program, options, func, cond->cond, vars, cond_ty, false);

            VarList then_vars = *vars;
            VarList otherwise_vars = *vars;
            if (cond->otherwise != NULL) {
                then_vars.length = 0;
                then_vars.capacity = 0;
                then_vars.elements = NULL;
                while (then_vars.length < vars->length) {
                    list_append(&then_vars, vars->elements[then_vars.length]);
                }
                otherwise_vars.length = 0;
                otherwise_vars.capacity = 0;
                otherwise_vars.elements = NULL;
                while (otherwise_vars.length < vars->length) {
                    list_append(&otherwise_vars, vars->elements[otherwise_vars.length]);
                }
            } else {
                then_vars = *vars;
            }

            if (cond->otherwise == NULL) {
                TypeValue* unit_ty = gen_typevalue("::core::types::unit", &cond->cond->span);
                resolve_typevalue(program, options, func->module, unit_ty, func->generics, func->type_generics);
                fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, unit_ty);
            }
            resolve_block(program, options, func, cond->then, &then_vars, t_return);
            if (cond->otherwise != NULL) {
                resolve_block(program, options, func, cond->otherwise, &otherwise_vars, t_return);
            }

            if (cond->otherwise != NULL) {
                for(usize i = 0;i < vars->length;i++) {
                    if (vars->elements[i].state == VS_COPY) continue;
                    StackVar then_var = then_vars.elements[i];
                    StackVar otherwise_var = then_vars.elements[i];
                    if (then_var.state == VS_MOVED) {
                        vars->elements[i].state = VS_MOVED;
                        vars->elements[i].moved_at = then_var.moved_at;
                        continue;
                    }
                    if (otherwise_var.state == VS_MOVED) {
                        vars->elements[i].state = VS_MOVED;
                        vars->elements[i].moved_at = otherwise_var.moved_at;
                        continue;
                    }
                }
                free(then_vars.elements);
                free(otherwise_vars.elements);
            } else {
                *vars = then_vars;
            }
        } break;
        case EXPR_WHILE_LOOP: {
            usize old_loop_index = vars->loop_index;
            vars->loop_index = vars->length;
            bool old_break_flag = vars->break_flag;
            vars->break_flag = false;
            Variable* old_break_move = vars->break_move;
            vars->break_move = NULL;
            
            WhileLoop* wl = expr->expr;
            TypeValue* bool_ty = gen_typevalue("::core::types::bool", &wl->cond->span);
            resolve_typevalue(program, options, func->module, bool_ty, func->generics, func->type_generics);
            TVBox* condbox = new_tvbox();
            condbox->type = bool_ty;
            resolve_expr(program, options, func, wl->cond, vars, condbox, false);
            TypeValue* unit_ty = gen_typevalue("::core::types::unit", &wl->body->span);
            resolve_typevalue(program, options, func->module, unit_ty, func->generics, func->type_generics);
            TVBox* bodybox = new_tvbox();
            bodybox->type = unit_ty;
            resolve_block(program, options, func, wl->body, vars, bodybox);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, unit_ty);            
        
            vars->loop_index = old_loop_index;
            vars->break_flag = old_break_flag;
            vars->break_move = old_break_move;
        } break;
        case EXPR_RETURN: {
            bool old_return_flag = vars->return_flag;
            vars->return_flag = true;
            Variable* old_return_move = vars->return_move;
            vars->return_move = NULL;

            VarList return_vars = *vars;
            return_vars.length = 0;
            return_vars.capacity = 0;
            return_vars.elements = NULL;
            while (return_vars.length < vars->length) {
                list_append(&return_vars, vars->elements[return_vars.length]);
            }

            Expression* ret = expr->expr;
            TypeValue* unit_ty = gen_typevalue("::core::types::unit", &expr->span);
            resolve_typevalue(program, options, func->module, unit_ty, func->generics, func->type_generics);
            if (ret == NULL) {
                // we return unit
                assert_types_equal(program, options, func->module, func->return_type, unit_ty, expr->span, func->generics, func->type_generics);
            } else {
                TVBox* return_type = new_tvbox();
                return_type->type = func->return_type;
                resolve_expr(program, options, func, ret, &return_vars, return_type, false);
            }
            // expression yields unit
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, unit_ty);            
        
            if (!vars->return_flag) { // we are not sure whether we return or not, so we need to keep the new vars
                VarList temp = *vars;
                *vars = return_vars;
                return_vars = temp;
            }
            free(return_vars.elements);
            vars->return_flag = old_return_flag;
            vars->return_move = old_return_move;
        } break;
        case EXPR_BREAK: {
            if (expr->expr != NULL) {
                resolve_expr(program, options, func, expr->expr, vars, t_return, false);
                todo("XPR_BREAK with expr");
            }
            TypeValue* unit_ty = gen_typevalue("::core::types::unit", &expr->span);
            resolve_typevalue(program, options, func->module, unit_ty, func->generics, func->type_generics);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, unit_ty);    
        
            vars->return_flag = false;
            if (vars->return_move != NULL) spanned_error("Conditional return", vars->return_move->path->elements.elements[0]->span, "Cannot move this variable since the `break` makes the return conditional and the variable may be accessed after the move");
        } break;
        case EXPR_CONTINUE: {
            TypeValue* unit_ty = gen_typevalue("::core::types::unit", &expr->span);
            resolve_typevalue(program, options, func->module, unit_ty, func->generics, func->type_generics);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, unit_ty);            
        
            vars->return_flag = false;
            vars->break_flag = false;
            if (vars->return_move != NULL) spanned_error("Conditional return", vars->return_move->path->elements.elements[0]->span, "Cannot move this variable since the `continue` makes the return conditional and the variable may be accessed after the move");
            if (vars->break_move != NULL) spanned_error("Conditional break", vars->return_move->path->elements.elements[0]->span, "Cannot move this variable since the `continue` makes the break conditional and the the variable may be accessed after the move");
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            TVBox* object_type = new_tvbox();
            resolve_expr(program, options, func, fa->object, vars, object_type, true);
            TypeValue* reference = gen_typevalue("::core::types::ptr<_>", &expr->span);
            resolve_typevalue(program, options, func->module, reference, NULL, NULL);
            TypeValue* tv = fa->object->resolved->type;
            if (reference->def == tv->def) { // is a reference;
                fa->is_ref = true;
                tv = tv->generics->generics.elements[0];
            }
            finish_tvbox(program, options, object_type, func);
            TypeDef* td = tv->def;
            if (td->module == NULL) spanned_error("Trying to access field of generic", fa->field->span, "%s @ %s is generic and as such has no visible fields",  to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(stream, fprint_span(stream, &td->name->span)));
            if (td->fields == NULL) unreachable("Fields should not be null unless it's a generic");
            Field* field = map_get(td->fields, fa->field->name);
            if (field == NULL) {
                if (td->module == NULL) unreachable("Module should not be null unless it's a generic");
                str path = to_str_writer(stream, {
                    fprint_path(stream, td->module->path);
                    fprintf(stream, "::%s", td->name->name);
                });
                if (str_eq(path, "::core::types::ptr")) {
                    spanned_error("Invalid struct field", fa->field->span, "%s @ %s has no such field '%s'. Try dereferencing the ptr to get the field of the inner type: `(*ptr).%s`.", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &td->name->span)), fa->field->name, fa->field->name);
                } else {
                    spanned_error("Invalid struct field", fa->field->span, "%s @ %s has no such field '%s'", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &td->name->span)), fa->field->name);
                }
            }
            if (field->type->def == NULL) resolve_typevalue(program, options, func->module, field->type, NULL, td->generics);
            TypeValue* field_ty = replace_generic(program, options, field->type, tv->generics, NULL, NULL, NULL);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, field_ty);
            if (!asref && !map_contains(field_ty->trait_impls, program->raii.copy_key) 
             && !list_contains(&field_ty->def->traits, i, TraitDef* trait, trait == program->raii.copy)) spanned_error("Cannot move field", fa->field->span, "Field `%s` of %s with type %s does not implement core::copy::Copy.\nImplement this trait or take a reference instead of moving", fa->field->name, to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_typevalue(s, field_ty)));
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            resolve_typevalue(program, options, func->module, slit->type, func->generics, func->type_generics);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, slit->type);
            
            slit->type = t_return->type;
            if (t_return->type->def == NULL) spanned_error("Type could not be inferred", expr->span, "%s is not a complete type", to_str_writer(s, fprint_typevalue(s, t_return->type)));
            TypeDef* type = slit->type->def;

            Map* temp_fields = map_new();
            map_foreach(type->fields, str key, Field* field, {
                resolve_typevalue(program, options, type->module, field->type, NULL, type->generics);
                map_put(temp_fields, key, field);
            });
            map_foreach(slit->fields, str key, StructFieldLit* field, {
                Field* f = map_remove(temp_fields, key);
                if (f == NULL) spanned_error("Invalid struct field", field->name->span, "Struct %s has no such field '%s'", type->name->name, key);
                TypeValue* field_ty = replace_generic(program, options, f->type, slit->type->generics, NULL, NULL, NULL);
                TVBox* fieldbox = new_tvbox();
                fieldbox->type = field_ty;
                resolve_expr(program, options, func, field->value, vars, fieldbox, false);
                patch_generics(replace_generic(program, options, f->type, slit->type->generics, NULL, NULL, NULL), f->type, fieldbox->type, NULL, slit->type->generics);
                finish_tvbox(program, options, fieldbox, func);
            });
            map_foreach(temp_fields, str key, Field* field, {
                UNUSED(field);
                spanned_error("Field not initialized", expr->span, "Field '%s' of struct %s was not initialized", key, type->name->name);
            });
        
            validate_item(program, options, slit->type->generics, NULL, type->generics, NULL, type->generics);        
        } break;
        case EXPR_C_INTRINSIC: {
            CIntrinsic* ci = expr->expr;
            usize i = 0;
            usize len = strlen(ci->c_expr);
            char op = '\0';
            char mode = '\0';
            while (i < len) {
                char c = ci->c_expr[i++];
                if (op != '\0' && mode == '\0') {
                    mode = c;
                } else if (op != '\0') {
                    if (op == '@') {
                        if (c != '[') spanned_error("Invalid c intrinsic", expr->span, "Expected `@%c[Type]`", mode);
                        usize start_i = i;
                        String key = list_new(String);
                        while (true) {
                            char c = ci->c_expr[i++];
                            if (c == '\0') spanned_error("Invalid c intrinsic", expr->span, "Expected `@%c[Type]`", mode);
                            if (c == ']') break;
                            list_append(&key, c);
                        }
                        list_append(&key, '\0');
                        TypeValue* tv = gen_typevalue(key.elements, &expr->span);
                        resolve_typevalue(program, options, func->module, tv, func->generics, func->type_generics);
                        list_append(&ci->type_bindings, tv);
                        list_append(&ci->binding_sizes, i - start_i);
                        if (mode != '.' && mode != ':' && mode != '!' && mode != '#') spanned_error("Invalid c intrinsic op", expr->span, "No such mode for intrinsci operator `%c%c`", op, mode);
                    } else if (op == '$') {
                        if (c != '[') spanned_error("Invalid c intrinsic", expr->span, "Expected `$%c[Type]`", mode);
                        usize start_i = i;
                        String key = list_new(String);
                        while (true) {
                            char c = ci->c_expr[i++];
                            if (c == '\0') spanned_error("Invalid c intrinsic", expr->span, "Expected `$%c[Type]`", mode);
                            if (c == ']') break;
                            list_append(&key, c);
                        }
                        list_append(&key, '\0');
                        Variable* v = malloc(sizeof(Variable));
                        v->box = NULL;
                        v->global_ref = NULL;
                        v->method_name = NULL;
                        v->values = NULL;
                        v->method_values = NULL;
                        Identifier* ident = malloc(sizeof(Identifier));
                        ident->name = key.elements;
                        ident->span = expr->span;
                        v->path = path_simple(ident);
                        find_variable(program, options, func->module, vars, v, func->generics, func->type_generics);
                        list_append(&ci->var_bindings, v);
                        list_append(&ci->binding_sizes, i - start_i);
                        if (mode != ':' && mode != '!') spanned_error("Invalid c intrinsic op", expr->span, "No such mode for intrinsic operator `%c%c`", op, mode);
                    } else spanned_error("Invalid c intrinsic op", expr->span, "No such intrinsic operator `%c`", op);
                    op = '\0';
                    mode = '\0';
                } else if (c == '@' || c == '$') {
                    op = c;
                };
            }
            if (op != '\0') spanned_error("Invalid c intrinsic", expr->span, "intrinsic ended on operator: `%s`", ci->c_expr);
            if (t_return->type == NULL) {
                TypeValue* unit_ty = gen_typevalue("::core::types::unit", &expr->span);
                resolve_typevalue(program, options, func->module, unit_ty, func->generics, func->type_generics);
                fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, unit_ty);
            }
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            TVBox* inner_tv = new_tvbox();
            if (t_return->type != NULL) {
                TypeValue* reference = gen_typevalue("::core::types::ptr<_>", &expr->span);
                resolve_typevalue(program, options, func->module, reference, NULL, NULL);
                if (reference->def != t_return->type->def) spanned_error("Expected resulting type to be a reference", expr->span, "Type %s, is not a reference, expected %s as a result", to_str_writer(s, fprint_typevalue(s, t_return->type)),to_str_writer(s, fprint_typevalue(s, reference)));
                inner_tv->type = t_return->type->generics->generics.elements[0];
            }
            resolve_expr(program, options, func, inner, vars, inner_tv, true);
            finish_tvbox(program, options, inner_tv, func);
            TypeValue* reference = gen_typevalue("::core::types::ptr::<_>", &expr->span);
            reference->generics->generics.elements[0] = inner->resolved->type;
            resolve_typevalue(program, options, func->module, reference, NULL, NULL);
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, reference);
        } break;
        case EXPR_DEREF: {
            Expression* inner = expr->expr;
            TVBox* inner_tv = new_tvbox();
            if (t_return->type != NULL) {
                inner_tv->type = gen_typevalue("::core::types::ptr::<_>", &expr->span);
                inner_tv->type->generics->generics.elements[0] = t_return->type;
                resolve_typevalue(program, options, func->module, inner_tv->type, func->generics, func->type_generics);
            }
            resolve_expr(program, options, func, inner, vars, inner_tv, asref);
            finish_tvbox(program, options, inner_tv, func);
            if (!str_eq(to_str_writer(s, fprint_td_path(s, inner_tv->type->def)), "::core::types::ptr")) spanned_error("Expected ptr to dereference", expr->span, "Cannot dereference type %s, expected ::core::types::ptr<_>", to_str_writer(s, fprint_typevalue(s, inner->resolved->type)));
            if (inner->resolved->type->generics == NULL || inner->resolved->type->generics->generics.length != 1) spanned_error("Expected ptr to have a pointee", expr->span, "Pointer %s should have one generic argument as its pointee", to_str_writer(s, fprint_typevalue(s, inner->resolved->type)));
            fill_tvbox(program, options, func->module, expr->span, func->generics, func->type_generics, t_return, inner->resolved->type->generics->generics.elements[0]);
            if (!asref && !map_contains(inner_tv->type->trait_impls, program->raii.copy_key) 
             && !list_contains(&inner_tv->type->def->traits, i, TraitDef* trait, trait == program->raii.copy)) spanned_error("Dereferencing non-copy type", inner->span, "Cannot dereference pointer as %s does not implement core::copy::Copy", to_str_writer(s, fprint_typevalue(s, t_return->type)));
        } break;
        default:
            unreachable("%s", ExprType__NAMES[expr->type]);
    }
    CURRENT_EXPR = last;
}

void resolve_block(Program* program, CompilerOptions* options, FuncDef* func, Block* block, VarList* vars, TVBox* t_return) {
    usize restore_len = vars->length;
    
    list_foreach(&block->expressions, i, Expression* expr, {
        if (block->yield_last && i == block->expressions.length - 1) {
            resolve_expr(program, options, func, expr, vars, t_return, false);
        } else {
            TVBox* expr_box = new_tvbox();
            resolve_expr(program, options, func, expr, vars, expr_box, false);
            finish_tvbox(program, options, expr_box, func);
        }
    });

    TVBox* yield_ty;
    if (block->yield_last && block->expressions.length > 0) {
        yield_ty = block->expressions.elements[block->expressions.length-1]->resolved;
    } else {
        yield_ty = malloc(sizeof(TVBox));
        yield_ty->type = gen_typevalue("::core::types::unit", &block->span);
        resolve_typevalue(program, options, func->module, yield_ty->type, func->generics, func->type_generics);
    }
    block->res = yield_ty->type;
    fill_tvbox(program, options, func->module, block->span, func->generics, func->type_generics, t_return, yield_ty->type);
    
    finish_tvbox(program, options, yield_ty, func);
    
    while (vars->length > restore_len) {
        vars->length -= 1;
        StackVar last = vars->elements[vars->length];
        if (last.state == VS_NONCOPY) { // not copy but also not moved -> need to drop
            list_append(&block->dropped, last.var);
        }
    }
}

void resolve_generic_keys(Program* program, CompilerOptions* options, Module* module, GenericKeys* keys, GenericKeys* trait_keys, GenericKeys* func_generics, GenericKeys* type_generics) {
    if (keys == NULL) return;
    if (map_size(keys->resolved) > 0) return;
    if (trait_keys != NULL && keys->generics.length != trait_keys->generics.length) spanned_error("Generic count does not match template", keys->span, "Got %lld generics, expected %lld @ %s", keys->generics.length, trait_keys->generics.length, to_str_writer(s, fprint_span(s, &trait_keys->span)));
    list_foreach(&keys->generics, i, GKey* key, {
        TypeDef* type;
        if (trait_keys == NULL) {
            type = malloc(sizeof(TypeDef));
            type->key = key;
            type->generics = NULL;
            type->name = key->name;
            type->extern_ref = NULL;
            type->fields = map_new();
            type->module = NULL;
        } else {
            str name = trait_keys->generics.elements[i]->name->name;
            type = map_get(trait_keys->resolved, name);
        }
        type->traits = list_new(TraitList);
        list_foreach(&key->bounds, i, TraitBound* bound, {
            TraitDef* trait = resolve_item(program, options, module, bound->bound->name, MIT_TRAIT, func_generics, type_generics, &bound->bound->generics);
            bound->resolved = trait;
            list_append(&type->traits, trait);
        });
        map_put(keys->resolved, key->name->name, type);
    });
}

void resolve_funcdef_head(Program* program, CompilerOptions* options, FuncDef* func) {
    if (options->verbosity >= 3) {
        if (func->trait_def) log("Resolving trait method definition %s::%s::%s", to_str_writer(s, fprint_path(s, func->trait->module->path)), func->trait->name->name, func->name->name);
        else if (func->impl_type == NULL) log("Resolving function %s::%s%s", to_str_writer(s, fprint_path(s, func->module->path)), func->name->name, to_str_writer(s, fprint_generic_keys(s, func->generics)));
        else if (func->trait != NULL) log("Resolving method %s::%s%s in trait %s::%s", to_str_writer(s, fprint_typevalue(s, func->impl_type)), func->name->name, to_str_writer(s, fprint_generic_keys(s, func->generics)), to_str_writer(s, fprint_path(s, func->trait->module->path)), func->trait->name->name);
        else log("Resolving method %s::%s%s", to_str_writer(s, fprint_typevalue(s, func->impl_type)), func->name->name, to_str_writer(s, fprint_generic_keys(s, func->generics)));
    }

    list_foreach(&func->annotations, i, Annotation a, {
        str p = to_str_writer(s, fprint_path(s, a.path));
        if (str_eq(p, "no_trace")) {            
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[no_trace]`, found %s", AnnotationType__NAMES[a.type]);
            if (func->untraced) spanned_error("Duplicate annotation flag", a.path->elements.elements[0]->span, "Flag `#[no_trace]` already set");
            func->untraced = true;
        } else if (str_eq(p, "extern")) {            
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[extern]`, found %s", AnnotationType__NAMES[a.type]);
            if (func->is_extern) spanned_error("Duplicate annotation flag", a.path->elements.elements[0]->span, "Flag `#[extern]` already set");
            func->is_extern = true;
        } 
    });
    if (!func->trait_def) {
        if (func->body != NULL) {
            if (func->is_extern) spanned_error("Local function marked as extern", func->name->span, "Function is marked as extern but also has a body");
        } else {
            if (!func->is_extern) spanned_error("Extern function not marked as such", func->name->span, "Extern function should have the appropiate tag: `#[extern]`");
        }
    }

    if (str_eq(func->name->name, "_")) spanned_error("Invalid func name", func->name->span, "`_` is a reserved name.");
    if (func->return_type == NULL) {
        func->return_type = gen_typevalue("::core::types::unit", &func->name->span);
    }
    resolve_generic_keys(program, options, func->module, func->type_generics, NULL, NULL, NULL);
    if (func->trait_def) {
        resolve_generic_keys(program, options, func->module, func->generics, NULL, NULL, func->type_generics);
    } else if (func->trait != NULL) {
        ModuleItem* template_mi = map_get(func->trait->methods, func->name->name);
        FuncDef* template = template_mi->item;
        if (func->generics->generics.length != template->generics->generics.length) spanned_error("Invalid trait impl", func->name->span, "Generic mismatch, the trait method shpould have %lld generics btugot %lld", template->generics->generics.length, func->generics->generics.length);
        resolve_generic_keys(program, options, func->module, func->generics, template->generics, NULL, func->type_generics);
    } else {
        resolve_generic_keys(program, options, func->module, func->generics, NULL, NULL, func->type_generics);
    }

    list_foreach(&func->args, i, Argument* arg, {
        resolve_typevalue(program, options, func->module, arg->type, func->generics, func->type_generics);
    });

    resolve_typevalue(program, options, func->module, func->return_type, func->generics, func->type_generics);
}

void resolve_funcdef_body(Program* program, CompilerOptions* options, FuncDef* func) {
    if (func->body_resolved) return;
    func->body_resolved = true;
    VarList vars = { 0 };
    list_foreach(&func->args, i, Argument* arg, {
        TVBox* box = new_tvbox();
        box->type = arg->type;
        register_variable(program, options, &vars, arg->var, box);
    });

    if(func->body == NULL) return;

    TVBox* blockbox = new_tvbox();
    // last expression is not a "return"?
    if (func->body->expressions.length > 0) {
        Expression* last = func->body->expressions.elements[func->body->expressions.length - 1];
        if (last->type != EXPR_RETURN) blockbox->type = func->return_type;
    }
    resolve_block(program, options, func, func->body, &vars, blockbox);
    finish_tvbox(program, options, blockbox, func);
    // last expression is not a "return"?
    if (func->body->expressions.length > 0 && func->body->yield_last) {
        Expression* last = func->body->expressions.elements[func->body->expressions.length - 1];
        if (last->type != EXPR_RETURN) {
            assert_types_equal(program, options, func->module, func->body->res, func->return_type, func->name->span, func->generics, func->type_generics);
        }
    }
}

void resolve_typedef_head(Program* program, CompilerOptions* options, TypeDef* type) {
    if (options->verbosity >= 3) log("Resolving type %s::%s", to_str_writer(s, fprint_path(s, type->module->path)), type->name->name);
    bool is_extern = false;
    list_foreach(&type->annotations, i, Annotation anno, {
        str name = to_str_writer(s, fprint_path(s, anno.path));
        if (str_eq(name, "extern")) {
            if (anno.type != AT_FLAG) spanned_error("Invalid annotation", anno.path->elements.elements[0]->span, "Expected annotation to look like #[extern]");
            if (is_extern) spanned_error("Duplicate annotation", anno.path->elements.elements[0]->span, "Struct already marked as #[extern]");
            if (type->fields != NULL) spanned_error("Extern struct", anno.path->elements.elements[0]->span, "Extern struct may not have a body");
            is_extern = true;
        } else if (str_eq(name, "c_alias")) {
            if (anno.type != AT_DEFINE) spanned_error("Invalid annotation", anno.path->elements.elements[0]->span, "Expected annotation to look like #[c_alias=\"alias_t\"]");
            if (!is_extern) spanned_error("Not extern", anno.path->elements.elements[0]->span, "Struct has to be marked as #[extern]");
            if (type->extern_ref != NULL) spanned_error("Duplicate annotation", anno.path->elements.elements[0]->span, "C alias already set: #[c_alias=\"%s\"]", type->extern_ref);
            Token* t = anno.data;
            if (t->type != STRING) spanned_error("Invalid type", t->span, "Expected c_alias to be a string, got %s", TokenType__NAMES[t->type]);
            type->extern_ref = t->string;
        }
    });
    if (!is_extern && type->fields == NULL) spanned_error("Extern type", type->name->span, "Extern types need #[extern] tag");
    if (is_extern && type->extern_ref == NULL) spanned_error("No c alias set", type->name->span, "An #[extern] struct needs a #[c_alias=\"alias_t\"]");
    if (type->extern_ref != NULL) {
        if (options->verbosity >= 3) log("Type %s is extern", type->name->name);
    }
    if (str_eq(type->name->name, "_")) spanned_error("Invalid type name", type->name->span, "`_` is a reserved name.");
    resolve_generic_keys(program, options, type->module, type->generics, NULL, NULL, NULL);
}

void resolve_typedef_body(Program* program, CompilerOptions* options, TypeDef* type) {
    if (type->fields == NULL) return;
    map_foreach(type->fields, str name, Field* field, {
        UNUSED(name);
        resolve_typevalue(program, options, type->module, field->type, NULL, type->generics);
    });
}

void register_impl(Program* program, CompilerOptions* options, ImplBlock* impl) {
    if (options->verbosity >= 5) {
        if (impl->trait_ref == NULL) log("registring name of impl block of %s", to_str_writer(s, fprint_typevalue(s, impl->type)));
        else log("registring name of impl block of %s on %s", to_str_writer(s, fprint_typevalue(s, impl->type)), to_str_writer(s, fprint_typevalue(s, impl->trait_ref)));
    }
    map_foreach(impl->methods, str name, ModuleItem* mi, ({
        switch (mi->type) {
            case MIT_FUNCTION: {
                FuncDef* method = mi->item;
                FuncList* list;
                if (map_contains(impl->module->package_method_map, name)) {
                    list = map_get(impl->module->package_method_map, name);
                } else {
                    list = malloc(sizeof(FuncList));
                    *list = list_new(FuncList);
                    map_put(impl->module->package_method_map, name, list);
                }
                list_append(list, method);
            } break;
            default:
                unreachable("%s cant be defined inside impl block", ModuleItemType__NAMES[mi->type]);
        }
    }));
    if (impl->trait_ref != NULL) {
        map_put(impl->module->package_trait_impl_map, to_str_writer(s, fprintf(s, "%p", impl)), impl);
    }
}

void resolve_impl_head(Program* program, CompilerOptions* options, ImplBlock* impl) {
    if (options->verbosity >= 4) {
        if (impl->trait_ref == NULL) log("Resolving impl block of %s", to_str_writer(s, fprint_typevalue(s, impl->type)));
        else log("Resolving impl block of %s on %s", to_str_writer(s, fprint_typevalue(s, impl->type)), to_str_writer(s, fprint_typevalue(s, impl->trait_ref)));
    }
    resolve_generic_keys(program, options, impl->module, impl->generics, NULL, NULL, NULL);
    resolve_typevalue(program, options, impl->module, impl->type, NULL, impl->generics);
    if (impl->trait_ref != NULL) {
        impl->trait = resolve_item(program, options, impl->module, impl->trait_ref->name, MIT_TRAIT, NULL, impl->generics, &impl->trait_ref->generics);
    }
    Module* def_root = impl->type->def->module;
    if (def_root == NULL) spanned_error("TODO: good error message", impl->type->def->name->span, "(impl on generic T)");
    while (def_root->parent != NULL) def_root = def_root->parent;
    Module* ctx_root = impl->module;
    while (ctx_root->parent != NULL) ctx_root = ctx_root->parent;
    if (def_root != ctx_root) spanned_error("Impl on foreign type", impl->type->name->elements.elements[0]->span, "Cannot impl in %s on %s defined in package %s", ctx_root->name->name, to_str_writer(s, fprint_typevalue(s, impl->type)), def_root->name->name);

    map_foreach(impl->methods, str name, ModuleItem* mi, ({
        UNUSED(name);
        switch (mi->type) {
            case MIT_FUNCTION: {
                FuncDef* method = mi->item;
                if (impl->trait != NULL) {
                    method->trait = impl->trait;
                }
                resolve_funcdef_head(program, options, method);
            } break;
            default:
                unreachable("%s cant be defined inside impl block", ModuleItemType__NAMES[mi->type]);
        }
    }));
}
void resolve_impl_body(Program* program, CompilerOptions* options, ImplBlock* impl) {
    // we are implementing the copy trait
    if (impl->trait == program->raii.copy) {
        // no tan extern type
        if (impl->type->def->fields != NULL) {
            map_foreach(impl->type->def->fields, str name, Field* field, {
                TypeValue* field_ty = replace_generic(program, options, field->type, impl->type->generics, NULL, NULL, NULL);
                if (!map_contains(field_ty->trait_impls, program->raii.copy_key) && !list_contains(&field_ty->def->traits, i, TraitDef* trait, trait == program->raii.copy)) {
                    spanned_error("Copy type with noncopy field", field->name->span, 
                    "Trying to implement core::copy::Copy on %s,\nbut field %s: %s is not Copy.\nFull impl type: %s\nFull impl field type: %s",
                    to_str_writer(s, fprint_typevalue(s, impl->type)), name, to_str_writer(s, fprint_typevalue(s, field_ty)),
                    to_str_writer(s, fprint_full_typevalue(s, impl->type)), to_str_writer(s, fprint_full_typevalue(s, field_ty)));
                }
            });
        }
    }

    map_foreach(impl->methods, str name, ModuleItem* mi, ({
        UNUSED(name);
        switch (mi->type) {
            case MIT_FUNCTION: {
                FuncDef* method = mi->item;
                resolve_funcdef_body(program, options, method);
            } break;
            default:
                unreachable("%s cant be defined inside impl block", ModuleItemType__NAMES[mi->type]);
        }
    }));
}

void resolve_trait_head(Program* program, CompilerOptions* options, TraitDef* trait) {
    if (options->verbosity >= 3) log("Resolving trait %s::%s", to_str_writer(s, fprint_path(s, trait->module->path)), trait->name->name);
    // insert self-type as dummy type and remove it afterwards
    ModuleItem* dummy = malloc(sizeof(ModuleItem));
    dummy->module = trait->module;
    dummy->origin = NULL;
    dummy->vis = V_PRIVATE; // in hopes of getting the least amount of trouble possible
    dummy->name = trait->self_type->name;
    dummy->type = MIT_STRUCT;
    dummy->item = trait->self_type;
    ModuleItem* old = map_put(trait->module->items, trait->self_key->name->name, dummy);

    resolve_generic_keys(program, options, trait->module, trait->keys, NULL, NULL, NULL);

    map_foreach(trait->methods, str name, ModuleItem* mi, ({
        UNUSED(name);
        switch (mi->type) {
            case MIT_FUNCTION: {
                FuncDef* func = mi->item;
                resolve_funcdef_head(program, options, func);
            } break;
            default:
                unreachable("%s cant be defined inside trait def", ModuleItemType__NAMES[mi->type]);
        }
    }));

    // remove dummy element again, reinserting the shadowed element
    if (old != NULL) {
        map_put(trait->module->items, trait->self_key->name->name, old);
    } else {
        map_remove(trait->module->items, trait->self_key->name->name);
    }
}

void resolve_trait_body(Program* program, CompilerOptions* options, TraitDef* trait) {
    map_foreach(trait->methods, str name, ModuleItem* mi, ({
        UNUSED(name);
        switch (mi->type) {
            case MIT_FUNCTION: {
                FuncDef* method = mi->item;
                if (method->body != NULL) todo("template functions in trait defs");
            } break;
            default:
                unreachable("%s cant be defined inside trait def", ModuleItemType__NAMES[mi->type]);
        }
    }));
}

// ive missed this typo so often that now its become intentional
void resovle_imports(Program* program, CompilerOptions* options, Module* module, List* mask) {
    if (mask == NULL) {
        mask = malloc(sizeof(List));
        *mask = list_new(List);
    }
    if (list_contains(mask, i, void* m, m == module)) return;
    list_append(mask, module);

    list_foreach(&module->imports, i, Import* import, {
        if (import->wildcard) {
            if (import->alias != NULL) unreachable("`use path::* as foo;` ... what is that supposed ot mean?");
            Module* container = resolve_item_raw(program, options, import->container, import->path, MIT_MODULE, mask)->item;
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
                                                                    ModuleItemType__NAMES[orig->type], to_str_writer(s, fprint_span(s, &orig->name->span)),
                                                                    ModuleItemType__NAMES[item->type], to_str_writer(s, fprint_span(s, &item->name->span)));
                }
                map_put(module->items, item->name->name, imported);
            });
        } else {
            ModuleItem* item = resolve_item_raw(program, options, import->container, import->path, MIT_ANY, mask);
            if (item->vis < import->vis && !(item->origin != NULL && import->path->elements.length == 1 && item->origin->vis >= import->vis)) spanned_error("Visibility error", import->path->elements.elements[0]->span, "Cannot reexport %s @ %s as %s while it is only declared as %s",
                    item->name->name, to_str_writer(s, fprint_span(s, &item->name->span)), Visibility__NAMES[import->vis], Visibility__NAMES[item->vis]);
            ModuleItem* imported = malloc(sizeof(ModuleItem));
            imported->vis = import->vis;
            if (item->origin != NULL && import->path->elements.length == 1) imported->vis = item->origin->vis;
            imported->type = item->type;
            imported->module = import->container;
            imported->origin = item;
            imported->item = item->item;
            Identifier* oname = item->name;
            if (import->alias != NULL) oname = import->alias;
            imported->name = oname;
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

void resolve_module_imports(Program* program, CompilerOptions* options, Module* module) {
    str path_str = to_str_writer(stream, fprint_path(stream, module->path));
    if (options->verbosity >= 3) log("resolving imports of %s", path_str);
    resovle_imports(program, options, module, NULL);
    map_foreach(module->items, str key, ModuleItem* item, {
        UNUSED(key);
        switch (item->type) {
            case MIT_MODULE: {
                if (item->origin == NULL) resolve_module_imports(program, options, item->item);
            } break;
            default: 
                break;
        }
    });
}
Token* const_eval(Expression* expr) {
    Token* result = malloc(sizeof(Token));
    result->span = expr->span;
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            Token* left = const_eval(op->lhs);
            Token* right = const_eval(op->rhs);
            if (left->type != NUMERAL) spanned_error("Invalid const eval binop argument type", left->span, "Can only const evaluate `%s` on number-like values, not %s", op->op, TokenType__NAMES[left->type]);
            if (right->type != NUMERAL) spanned_error("Invalid const eval binop argument type", left->span, "Can only const evaluate `%s` on number-like values, not %s", op->op, TokenType__NAMES[left->type]);
            str endptr = NULL;
            i64 l = strtoll(left->string, &endptr, 0);
            if (endptr == NULL) spanned_error("Invalid const eval number", left->span, "Could not convert `%s` to an integer.\nPlease note that const eval does not suppport type suffixes.", left->string);
            i64 r = strtoll(right->string, &endptr, 0);
            if (endptr == NULL) spanned_error("Invalid const eval number", left->span, "Could not convert `%s` to an integer.\nPlease note that const eval does not suppport type suffixes.", left->string);
            i64 x = 0;
            if (str_eq(op->op, "+")) {
                x = l + r;
            } else if (str_eq(op->op, "-")) {
                x = l - r;
            } else if (str_eq(op->op, "*")) {
                x = l * r;
            } else if (str_eq(op->op, "/")) {
                x = l / r;
            } else if (str_eq(op->op, "&")) {
                x = l & r;
            } else if (str_eq(op->op, "|")) {
                x = l ^ r;
            } else if (str_eq(op->op, "^")) {
                x = l ^ r;
            } else if (str_eq(op->op, "%")) {
                x = l % r;
            } else if (str_eq(op->op, ">>")) {
                x = l >> r;
            } else if (str_eq(op->op, "<<")) {
                x = l << r;
            } else if (str_eq(op->op, ">")) {
                x = l > r;
            } else if (str_eq(op->op, ">=")) {
                x = l >= r;
            } else if (str_eq(op->op, "<")) {
                x = l < r;
            } else if (str_eq(op->op, "<=")) {
                x = l <= r;
            } else if (str_eq(op->op, "==")) {
                x = l == r;
            } else if (str_eq(op->op, "!=")) {
                x = l != r;
            } else if (str_eq(op->op, "&&")) {
                x = l && r;
            } else if (str_eq(op->op, "||")) {
                x = l || r;
            } else {
                spanned_error("Invalid const eval operation", op->op_span, "Cannot perform `%s` at compile time", op->op);
            }
            result->string = to_str_writer(s, fprintf(s, "%lld", x));
            result->type = NUMERAL;
            break;
        } case EXPR_LITERAL: {
            Token* t = expr->expr;
            result->string = t->string;
            result->type = t->type;
            break;
        } default:
            spanned_error("Invalid constant operation", expr->span, "Cannot evaluate expression of type %s at compile time", ExprType__NAMES[expr->type]);
    }
    return result;
}

void resolve_global(Program* program, CompilerOptions* options, Global* g) {
    if (options->verbosity >= 3) {
        if(g->constant) log("Resolving constant %s::%s", to_str_writer(s, fprint_path(s, g->module->path)), g->name->name);
        else log("Resolving static %s::%s", to_str_writer(s, fprint_path(s, g->module->path)), g->name->name);
    }
    resolve_typevalue(program, options, g->module, g->type, NULL, NULL);
    if (g->value != NULL) {
        g->computed_value = const_eval(g->value);
    } else {
        if (g->constant) spanned_error("Uninitialized constant", g->name->span, "Constant needs an associasted value.\nNote that this is optional for statics.");
    }
}

static void resolve_rec(Program* program, CompilerOptions* options, Module* module, ModuleItemType type, resolver_fn resolver) {
    if (type == MIT_MODULE) panic("Can only resolve items in modules recursively, not modules themselves");
    map_foreach(module->items, str name, ModuleItem* item, {
        UNUSED(name);
        if (item->origin != NULL) continue; // foreign item
        if (item->type == type) resolver(program, options, item->item);
        else if (item->type == MIT_MODULE) resolve_rec(program, options, item->item, type, resolver);
    });
}

static void resolve_rec_impl(Program* program, CompilerOptions* options, Module* module, resolver_fn resolver) {
    list_foreach(&module->impls, i, ImplBlock* impl, resolver(program, options, impl));
    map_foreach(module->items, str name, ModuleItem* item, {
        UNUSED(name);
        if (item->origin != NULL) continue; // foreign item
        if (item->type == MIT_MODULE) resolve_rec_impl(program, options, item->item, resolver);
    });
}

void resolve(Program* program, CompilerOptions* options) {
    program->raii.copy = resolve_item_raw(program, options, program->main_module, gen_path("::core::copy::Copy"), MIT_TRAIT, NULL)->item;
    program->raii.copy_key = to_str_writer(s, fprintf(s, "%p", program->raii.copy));
    program->raii.clone = resolve_item_raw(program, options, program->main_module, gen_path("::core::clone::Clone"), MIT_TRAIT, NULL)->item;
    program->raii.clone_key = to_str_writer(s, fprintf(s, "%p", program->raii.clone));
    program->raii.drop = resolve_item_raw(program, options, program->main_module, gen_path("::core::drop::Drop"), MIT_TRAIT, NULL)->item;
    program->raii.drop_key = to_str_writer(s, fprintf(s, "%p", program->raii.drop));
    program->raii.raw = resolve_item_raw(program, options, program->main_module, gen_path("::core::drop::Raw"), MIT_STRUCT, NULL)->item;
    
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Registring impl names");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec_impl(program, options, mod, (resolver_fn)register_impl);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving imports");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_module_imports(program, options, mod);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving typedef heads");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec(program, options, mod, MIT_STRUCT, (resolver_fn)resolve_typedef_head);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving trait heads");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec(program, options, mod, MIT_TRAIT, (resolver_fn)resolve_trait_head);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving impl heads");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec_impl(program, options, mod, (resolver_fn)resolve_impl_head);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving function heads");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec(program, options, mod, MIT_FUNCTION, (resolver_fn)resolve_funcdef_head);
    });

    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving typedef body");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec(program, options, mod, MIT_STRUCT, (resolver_fn)resolve_typedef_body);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving globals");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec(program, options, mod, MIT_STATIC, (resolver_fn)resolve_global);
        resolve_rec(program, options, mod, MIT_CONSTANT, (resolver_fn)resolve_global);
    });

    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving function body");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec(program, options, mod, MIT_FUNCTION, (resolver_fn)resolve_funcdef_body);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving impl body");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec_impl(program, options, mod, (resolver_fn)resolve_impl_body);
    });
    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_PURPLE_FG) "PASS" ANSI_RESET_SEQUENCE, "Resolving trait body");
    map_foreach(program->packages, str key, Module* mod, {
        UNUSED(key);
        resolve_rec(program, options, mod, MIT_TRAIT, (resolver_fn)resolve_trait_body);
    });

    program->tracegen.top_frame = malloc(sizeof(Variable));
    program->tracegen.top_frame->path = gen_path("::core::trace::TOP_FRAME");
    program->tracegen.top_frame->values = NULL;
    find_variable(program, options, program->main_module, NULL, program->tracegen.top_frame, NULL, NULL);
    program->tracegen.frame_type = gen_typevalue("::core::trace::Frame", NULL);
    resolve_typevalue(program, options, program->main_module, program->tracegen.frame_type, NULL, NULL);
    program->tracegen.function_type = gen_typevalue("::core::trace::Function", NULL);
    resolve_typevalue(program, options, program->main_module, program->tracegen.function_type, NULL, NULL);
}