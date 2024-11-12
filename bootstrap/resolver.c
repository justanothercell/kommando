#include "resolver.h"
#include "ast.h"
#include "lib.h"
#include "lib/str.h"
#include "module.h"
#include "parser.h"
#include "token.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void fprint_res_tv(FILE* stream, TypeValue* tv) {
    if (tv->def == NULL) panic("TypeValue was not resolved %s @ %s", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &tv->name->elements.elements[0]->span)));
    if (tv->def->extern_ref != NULL) {
        fprintf(stream, "%s", tv->def->extern_ref);
        return;
    }
    fprintf(stream, "%s%p", tv->def->name->name, tv->def);
    if (tv->generics != NULL && tv->generics->generics.length > 0) {
        fprintf(stream, "<");
        list_foreach_i(&tv->generics->generics, lambda(void, usize i, TypeValue* tv, {
            if (i > 0) fprintf(stream, ",");
            fprint_res_tv(stream, tv);
        }));
        fprintf(stream, ">");
    }
}

static str __gvals_to_key(GenericValues* generics, bool assert_resolved) {
    return to_str_writer(stream, {
        fprintf(stream, "#");
        if (generics != NULL && generics->generics.length > 0) {
            fprintf(stream, "<");
            list_foreach_i(&generics->generics, lambda(void, usize  i, TypeValue* tv, {
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
                    fprintf(stream, "@<%s>", to_str_writer(stream, {
                        list_foreach_i(&tv->ctx->generics, lambda(void, usize i, Identifier* g, {
                            if (i > 0) fprintf(stream, ",");
                            str key = g->name;
                            TypeDef* td = map_get(tv->ctx->resolved, key);
                            fprintf(stream, "%s%p", td->name->name, td);
                        }));
                    }));
                }
            }));
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

static ModuleItem* resolve_item_raw(Program* program, Module* module, Path* path, ModuleItemType kind) {
    Identifier* name = path->elements.elements[path->elements.length-1];

    if (path->elements.length == 1 && !path->absolute) goto get_item;
    
    usize i = 0;
    Identifier* m = path->elements.elements[0];
    ModuleItem* mod = NULL;
    if (!path->absolute) mod = map_get(module->items, m->name);
    if (mod != NULL) { // try local
        if (mod->type != MIT_MODULE) spanned_error("Is not a module", m->span, "Item %s is of type %s, expected it to be a module", m->name, ModuleItemType__NAMES[mod->type]);
        module = mod->item;
        i += 1;
    } else { // try global
        Module* package = map_get(program->packages, m->name);
        if (package != NULL) {
            i += 1;
            module = package;
        } else if (path->absolute) spanned_error("No such package", m->span, "Package %s does not exist. Try including it with the command line args: ::%s=path/to/package.kdo", m->name, m->name);
    }
    if (i == 0) spanned_error("No such module", m->span, "Module %s does not exist.", m->name);
    while (i < path->elements.length - 1) {
        Identifier* m = path->elements.elements[i];
        i += 1;
        ModuleItem* it = map_get(module->items, m->name);
        log("%p %s", it, to_str_writer(s, fprint_path(s, path)));
        if (it == NULL) spanned_error("No such module", m->span, "Module %s does not exist.", m->name);
        log("%s", to_str_writer(s, fprint_span(s, &m->span)));
        log("%u %u", it->type, MIT_MODULE);
        log("%p", it->item);
        log("%p", it);
        if (it->type != MIT_MODULE) spanned_error("Is not a module", m->span, "Item %s is of type %s, expected it to be a module", m->name, ModuleItemType__NAMES[it->type]);
        module = it->item;
    }
    get_item: {}
    ModuleItem* it = map_get(module->items, name->name);
    if (it == NULL) spanned_error("No such item", name->span, "%s %s does not exist.", ModuleItemType__NAMES[kind], name->name);
    if (it->type != kind) spanned_error("Itemtype mismatch", name->span, "Item %s is of type %s, expected it to be %s", name->name, ModuleItemType__NAMES[it->type], ModuleItemType__NAMES[kind]);
    return it;
}

void register_item(GenericValues* item_values, GenericKeys* gkeys) {
    // no need to register if we are not generic - we just complie the one default nongeneric variant in that case
    if (item_values != NULL) {
        str key = gvals_to_key(item_values);
        log("trying to register %s", key);
        if (!map_contains(gkeys->generic_uses, key)) {
            log("Registered!");
            map_put(gkeys->generic_uses, key, item_values);
            list_append(&gkeys->generic_use_keys, key);
        }
    }
}

void resolve_funcdef(Program* program, FuncDef* func);
void resolve_typedef(Program* program, TypeDef* ty);
void resolve_typevalue(Program* program, Module* module, TypeValue* tval, GenericKeys* func_generics, GenericKeys* type_generics);
void* resolve_item(Program* program, Module* module, Path* path, ModuleItemType kind, GenericKeys* func_generics, GenericKeys* type_generics, GenericValues** gvalsref) {
    typedef struct {
        str key;
        ModuleItem* item;
    } MICacheItem;
    LIST(MICList, MICacheItem);
    static MICList LOOKUP_CACHE = list_new(MICList);
    const usize CACHE_SIZE = 16;
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
        }
    }

    if (mi == NULL) {
        mi = resolve_item_raw(program, module, path, kind);
        MICacheItem mic = (MICacheItem) { .key=key, .item = mi };
        if (LOOKUP_CACHE.length < CACHE_SIZE) list_append(&LOOKUP_CACHE, mic);
        else LOOKUP_CACHE.elements[CACHE_SIZE - 1] = mic;
    }
    switch (mi->type) {
        case MIT_FUNCTION: {
            FuncDef* func = mi->item;
            if (!func->head_resolved) resolve_funcdef(program, func);
        } break;
        case MIT_STRUCT: {
            TypeDef* type = mi->item;
            if (!type->head_resolved) resolve_typedef(program, type);
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
        list_foreach(&gkeys->generics, lambda(void, Identifier* generic, {
            TypeValue* dummy = gen_typevalue("_", &span);
            list_append(&gvals->generics, dummy);
            map_put(gvals->resolved, generic->name, dummy);
        }));
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
        if (func_generics != NULL && generic_value->ctx != func_generics && map_contains(func_generics->resolved, generic_value->def->name->name)) {
            if (generic_value->ctx != NULL) spanned_error("Multiple context found", generic_value->name->elements.elements[0]->span, "%s has potential contexts in %s @ %s and %s @ %s",
                                                generic_value->def->name->name,
                                                to_str_writer(s, fprint_span_contents(s, &generic_value->ctx->span)), to_str_writer(s, fprint_span(s, &generic_value->ctx->span)),
                                                to_str_writer(s, fprint_span_contents(s, &func_generics->span)), to_str_writer(s, fprint_span(s, &func_generics->span))
                                            );
            generic_value->ctx = func_generics;
        }
        if (type_generics != NULL && generic_value->ctx != type_generics && map_contains(type_generics->resolved, generic_value->def->name->name)) {
            if (generic_value->ctx != NULL) spanned_error("Multiple context found", generic_value->name->elements.elements[0]->span, "%s has potential contexts in %s @ %s and %s @ %s",
                                                generic_value->def->name->name,
                                                to_str_writer(s, fprint_span_contents(s, &generic_value->ctx->span)), to_str_writer(s, fprint_span(s, &generic_value->ctx->span)),
                                                to_str_writer(s, fprint_span_contents(s, &type_generics->span)), to_str_writer(s, fprint_span(s, &type_generics->span))
                                            );
            generic_value->ctx = type_generics;
        }
    }
    generic_end: {}
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
        list_foreach(&tval->generics->generics, lambda(void, TypeValue* tv, {
            resolve_typevalue(program, module, tv, func_generics, type_generics);
        }));
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
typedef struct VarBox {
    str name;
    usize id;
    TVBox* resolved;
} VarBox;
LIST(VarList, VarBox*);
static VarBox* var_find(VarList* vars, Variable* var) {
    for (int i = vars->length-1;i >= 0;i--) {
        if (str_eq(vars->elements[i]->name, var->name->name)) return vars->elements[i];
    }
    spanned_error("No such variable", var->name->span, "`%s` does not exist", var->name->name);
}
static VarBox* var_register(VarList* vars, Variable* var) {
    VarBox* v = malloc(sizeof(VarBox));
    v->name = var->name->name;
    v->id = vars->length;
    v->resolved = malloc(sizeof(TVBox));
    v->resolved->type = NULL;
    var->id = v->id;
    list_append(vars, v);
    return v;
}

TypeValue* replace_generic(TypeValue* tv, GenericValues* ctx) {
    if (ctx == NULL) return tv;
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
        map_foreach(tv->generics->resolved, lambda(void, str key, TypeValue* val, {
            map_put(rev, to_str_writer(s, fprintf(s, "%p", val)), key);
        }));
        list_foreach(&tv->generics->generics, lambda(void, TypeValue* val, {
            str real_key = map_get(rev, to_str_writer(s, fprintf(s, "%p", val)));
            val = replace_generic(val, ctx);
            list_append(&clone->generics->generics, val);
            map_put(clone->generics->resolved, real_key, val);
        }));
        return clone;
    }
    if (tv->name->absolute || tv->name->elements.length != 1) return tv;
    if (map_contains(ctx->resolved, tv->name->elements.elements[0]->name)) {
        return map_get(ctx->resolved, tv->name->elements.elements[0]->name);
    }
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
            map_foreach(tv1->generics->resolved, lambda(void, str key, TypeValue* val, {
                map_put(rev1, to_str_writer(s, fprintf(s, "%p", val)), key);
            }));
            Map* rev2 = map_new();
            map_foreach(tv2->generics->resolved, lambda(void, str key, TypeValue* val, {
                map_put(rev2, to_str_writer(s, fprintf(s, "%p", val)), key);
            }));
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

//                  Foo<_>               Foo<T>            Foo<i32>          <T, U, V>
void patch_generics(TypeValue* template, TypeValue* match, TypeValue* value, GenericValues* generics) {
    if (!template->name->absolute && template->name->elements.length == 1 && str_eq(template->name->elements.elements[0]->name, "_")) {
        if (!match->name->absolute && match->name->elements.length == 1) {
            str generic = match->name->elements.elements[0]->name;

            if (!map_contains(generics->resolved, generic)) {
                spanned_error("No such generic", match->name->elements.elements[0]->span, "%s is not a valid generic for %s @ %s", to_str_writer(s, fprint_typevalue(s, match)), 
                    to_str_writer(s, fprint_span_contents(s, &generics->span)), to_str_writer(s, fprint_span(s, &generics->span))
                );
            }
            TypeValue* old = map_put(generics->resolved, generic, value);
            list_find_map(&generics->generics, tvref, *tvref == old, lambda(void, TypeValue** tvref, {
                *tvref = value;
            }));
            return;
        }
        spanned_error("Not a generic", match->name->elements.elements[0]->span, "%s is not a generic parameter and cannot substitute _ @ %s", to_str_writer(s, fprint_typevalue(s, match)), to_str_writer(s, fprint_span(s, &template->name->elements.elements[0]->span)));
    }
    if (!match->name->absolute && match->name->elements.length == 1 && match->generics == NULL) return;
    if (template->generics != NULL && match->generics != NULL && value->generics != NULL) {
        if (template->generics->generics.length == match->generics->generics.length && match->generics->generics.length == value->generics->generics.length) {
            for (usize i = 0;i < template->generics->generics.length;i++) {
                patch_generics(template->generics->generics.elements[i], match->generics->generics.elements[i], value->generics->generics.elements[i], generics);
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

void finish_tvbox(TVBox* box) {
    register_item(box->type->generics, box->type->def->generics);
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
                TypeValue* bool_ty = gen_typevalue("::std::bool", &op->op_span);
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
            log("calling %s in %s", fd->name->name, func->name->name);
            // preresolve return
            TypeValue* pre_ret = replace_generic(fd->return_type, fc->generics);
            if (t_return->type != NULL) patch_generics(pre_ret, fd->return_type, t_return->type, fc->generics);

            // checking arg count
            fc->def = fd;
            if (fd->is_variadic) {
                if (fd->args.length > fc->arguments.length) spanned_error("Too few args for variadic function", expr->span, "expected at least %llu args, got %llu", fd->args.length, fc->arguments.length);
            } else {
                if (fd->args.length != fc->arguments.length) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", fd->args.length, fc->arguments.length);
            }
            // resolving args
            list_foreach_i(&fc->arguments, lambda(void, usize i, Expression* arg, {
                log("%s in %s, arg %llu of %llu", fd->name->name, func->name->name, i, fd->args.length);
                if (i < fd->args.length) {
                    TVBox* argbox = new_tvbox();
                    TypeValue* arg_tv = replace_generic(fd->args.elements[i]->type, fc->generics);
                    argbox->type = arg_tv;
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    patch_generics(replace_generic(fd->args.elements[i]->type, fc->generics), fd->args.elements[i]->type, argbox->type, fc->generics);
                    finish_tvbox(argbox);
                } else { // variadic arg
                    TVBox* argbox = new_tvbox();
                    resolve_expr(program, func, type_generics, arg, vars, argbox);
                    finish_tvbox(argbox);
                }
            }));

            TypeValue* ret = replace_generic(fd->return_type, fc->generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, ret);

            log("@ %s", gvals_to_key(fc->generics));
            register_item(fc->generics, fd->generics);        
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING: {
                    t_return->type = gen_typevalue("::std::c_const_str_ptr", &expr->span);
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
                        tv = gen_typevalue(to_str_writer(stream, fprintf(stream, "::std::%s", ty)), &expr->span);
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
            VarBox* vb = var_find(vars, var);
            var->id = vb->id;
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, vb->resolved->type);
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;

            VarBox* v = var_register(vars, let->var);
            let->var->id = v->id;
            v->resolved = new_tvbox();
            if (let->type != NULL) {
                resolve_typevalue(program, func->module, let->type, func->generics, type_generics);
                fill_tvbox(program, func->module, expr->span, func->generics, type_generics, v->resolved, let->type);
            }
            resolve_expr(program, func, type_generics, let->value, vars, v->resolved);
            patch_tvs(&let->type, &v->resolved->type);

            TypeValue* tv = gen_typevalue("::std::unit", &expr->span);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, tv);
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            TVBox* assign_t = new_tvbox();
            resolve_expr(program, func, type_generics, assign->asignee, vars, assign_t);
            resolve_expr(program, func, type_generics, assign->value, vars, assign_t);

            TypeValue* tv = gen_typevalue("::std::unit", &expr->span);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, tv);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;

            TypeValue* bool_ty = gen_typevalue("::std::bool", &cond->cond->span);
            resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
            TVBox* cond_ty = new_tvbox();
            cond_ty->type = bool_ty;
            resolve_expr(program, func, type_generics, cond->cond, vars, cond_ty);

            if (cond->otherwise == NULL) {
                TypeValue* unit_ty = gen_typevalue("::std::unit", &cond->cond->span);
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
            TypeValue* bool_ty = gen_typevalue("::std::bool", &wl->cond->span);
            resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
            TVBox* condbox = new_tvbox();
            condbox->type = bool_ty;
            resolve_expr(program, func, type_generics, wl->cond, vars, condbox);
            TypeValue* unit_ty = gen_typevalue("::std::unit", &wl->body->span);
            resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
            TVBox* bodybox = new_tvbox();
            bodybox->type = unit_ty;
            resolve_block(program, func, type_generics, wl->body, vars, bodybox);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);            
        } break;
        case EXPR_RETURN: {
            Expression* ret = expr->expr;
            TypeValue* unit_ty = gen_typevalue("::std::unit", &expr->span);
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
            TypeValue* unit_ty = gen_typevalue("::std::unit", &expr->span);
            resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);    
        } break;
        case EXPR_CONTINUE: {
            TypeValue* unit_ty = gen_typevalue("::std::unit", &expr->span);
            resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, unit_ty);            
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            TVBox* object_type = new_tvbox();
            resolve_expr(program, func, type_generics, fa->object, vars, object_type);
            finish_tvbox(object_type);
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
            TypeValue* field_ty = replace_generic(field->type, tv->generics);
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
            map_foreach(type->fields, lambda(void, str key, Field* field, {
                map_put(temp_fields, key, field);
            }));
            map_foreach(slit->fields, lambda(void, str key, StructFieldLit* field, {
                Field* f = map_remove(temp_fields, key);
                if (f == NULL) spanned_error("Invalid struct field", field->name->span, "Struct %s has no such field '%s'", type->name->name, key);
                TypeValue* field_ty = replace_generic(f->type, slit->type->generics);
                TVBox* fieldbox = new_tvbox();
                fieldbox->type = field_ty;
                resolve_expr(program, func, type_generics, field->value, vars, fieldbox);
                patch_generics(replace_generic(f->type, slit->type->generics), f->type, fieldbox->type, slit->type->generics);
                finish_tvbox(fieldbox);
            }));
            map_foreach(temp_fields, lambda(void, str key, Field* field, {
                UNUSED(field);
                spanned_error("Field not initialized", slit->type->name->elements.elements[0]->span, "Field '%s' of struct %s was not initialized", type->name->name, key);
            }));
        
            register_item(slit->type->generics, type->generics);        
        } break;
        case EXPR_C_INTRINSIC: {
            CIntrinsic* ci = expr->expr;
            map_foreach(ci->var_bindings, lambda(void, str key, Variable* var, {
                UNUSED(key);
                VarBox* vb = var_find(vars, var);
                var->id = vb->id;
            }));
            map_foreach(ci->type_bindings, lambda(void, str key, TypeValue* tv, {
                UNUSED(key);
                resolve_typevalue(program, func->module, tv, func->generics, type_generics);
            }));
            resolve_typevalue(program, func->module, ci->ret_ty, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, ci->ret_ty);
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            TVBox* inner_tv = new_tvbox();
            if (t_return->type != NULL) {
                TypeValue* reference = gen_typevalue("::intrinsics::types::ptr<_>", &expr->span);
                resolve_typevalue(program, func->module, reference, func->generics, type_generics);
                if (reference->def != t_return->type->def) spanned_error("Expected resulting type to be a reference", expr->span, "Type %s, is not a reference, expected %s as a result", to_str_writer(s, fprint_typevalue(s, t_return->type)),to_str_writer(s, fprint_typevalue(s, reference)));
                inner_tv->type = t_return->type->generics->generics.elements[0];
            }
            resolve_expr(program, func, type_generics, inner, vars, inner_tv);
            finish_tvbox(inner_tv);
            TypeValue* reference = gen_typevalue("::intrinsics::types::ptr::<_>", &expr->span);
            reference->generics->generics.elements[0] = inner->resolved->type;
            resolve_typevalue(program, func->module, reference, func->generics, type_generics);
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
            finish_tvbox(inner_tv);
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
    
    list_foreach_i(&block->expressions, lambda(void, usize i, Expression* expr, {
        if (block->yield_last && i == block->expressions.length - 1) {
            resolve_expr(program, func, type_generics, expr, vars, t_return);
        } else {
            TVBox* expr_box = new_tvbox();
            resolve_expr(program, func, type_generics, expr, vars, expr_box);
            finish_tvbox(expr_box);
        }
    }));

    TVBox* yield_ty;
    if (block->yield_last && block->expressions.length > 0) {
        yield_ty = block->expressions.elements[block->expressions.length-1]->resolved;
    } else {
        yield_ty = malloc(sizeof(TVBox));
        yield_ty->type = gen_typevalue("::std::unit", &block->span);
        resolve_typevalue(program, func->module, yield_ty->type, func->generics, type_generics);
    }
    block->res = yield_ty->type;
    fill_tvbox(program, func->module, block->span, func->generics, type_generics, t_return, yield_ty->type);
    
    finish_tvbox(yield_ty);
    
    vars->length = restore_len;
}

void resolve_module(Program* program, Module* module);
void resolve_funcdef(Program* program, FuncDef* func) {
    if (!func->module->resolved && !func->module->in_resolution) {
        resolve_module(program, func->module);
    }
    log("Resolving function %s", func->name->name);
    if (str_eq(func->name->name, "_")) spanned_error("Invalid func name", func->name->span, "`_` is a reserved name.");
    if (func->return_type == NULL) {
        func->return_type = gen_typevalue("::std::unit", &func->name->span);
    }
    if (func->generics != NULL) {
        list_foreach(&func->generics->generics, lambda(void, Identifier* key, {
            TypeDef* type = malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = map_new();
            type->module = NULL;
            map_put(func->generics->resolved, key->name, type);
        }));
    }
    GenericKeys* type_generics = NULL;
    VarList vars = list_new(VarList);
    list_foreach(&func->args, lambda(void, Argument* arg, {
        VarBox* v = var_register(&vars, arg->var);
        v->resolved->type = arg->type;
        resolve_typevalue(program, func->module, arg->type, func->generics, type_generics);
    }));
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
        finish_tvbox(blockbox);
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
    log("Resolving type %s", ty->name->name);
    if (str_eq(ty->name->name, "_")) spanned_error("Invalid type name", ty->name->span, "`_` is a reserved name.");
    if (ty->generics != NULL) {
        list_foreach(&ty->generics->generics, lambda(void, Identifier* key, {
            TypeDef* type = malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = NULL;
            type->module = NULL;
            map_put(ty->generics->resolved, key->name, type);
        }));
    }
    ty->head_resolved = true;
    map_foreach(ty->fields, lambda(void, str name, Field* field, {
        UNUSED(name);
        resolve_typevalue(program, ty->module, field->type, NULL, ty->generics);
    }));
}

void resolve_module(Program* program, Module* module) {
    if (module->resolved) return;
    str path_str = to_str_writer(stream, fprint_path(stream, module->path));
    if (module->in_resolution) panic("recursion detected while resolving %s", path_str);
    module->in_resolution = true;

    log("Resolving module %s", path_str);

    map_foreach(module->items, lambda(void, str key, ModuleItem* item, {
        UNUSED(key);
        switch (item->type) {
            case MIT_FUNCTION: {
                FuncDef* func = item->item;
                if (func->head_resolved) return;
                resolve_funcdef(program, func);
            } break;
            case MIT_STRUCT: {
                TypeDef* type = item->item;
                if (type->head_resolved) return;
                resolve_typedef(program, type);
            } break;
            case MIT_MODULE: {
                Module* mod = item->item;
                resolve_module(program, mod);
            } break;
        }
    }));

    module->resolved = true;
    module->in_resolution = false;
}

void resolve(Program* program) {
    map_foreach(program->packages, lambda(void, str key, Module* mod, {
        UNUSED(key);
        resolve_module(program, mod);
    }));
}
