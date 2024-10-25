#include "resolver.h"
#include "ast.h"
#include "lib.h"
#include "lib/debug.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/gc.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
#include "module.h"
#include "parser.h"
#include "token.h"
#include <signal.h>
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

str gvals_to_c_key(GenericValues* generics) {
    if (generics != NULL && generics->generic_func_ctx != NULL) panic("Generic not fully resolved (func)");
    if (generics != NULL && generics->generic_type_ctx != NULL) panic("Generic not fully resolved (type)");
    return gvals_to_key(generics);
}
str gvals_to_key(GenericValues* generics) {
    return to_str_writer(stream, {
        fprintf(stream, "#");
        if (generics != NULL && generics->generics.length > 0) {
            fprintf(stream, "<");
            list_foreach_i(&generics->generics, lambda(void, usize  i, TypeValue* tv, {
                if (i > 0) fprintf(stream, ",");
                if (tv->def == NULL) panic("Unresolved typevalue %s @ %s", to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &tv->name->elements.elements[0]->span)));
                if (tv->def->module != NULL) {
                    fprint_path(stream, tv->def->module->path);
                    fprintf(stream, "::");
                }
                fprintf(stream, "%s", tv->def->name->name);
                fprintf(stream, "%s", gvals_to_key(tv->generics));
            }));
            fprintf(stream, ">");
            if (generics->generic_type_ctx != NULL) {
                fprintf(stream, "@T<%s>", to_str_writer(stream, {
                    list_foreach_i(&generics->generic_type_ctx->generics, lambda(void, usize i, Identifier* g, {
                        if (i > 0) fprintf(stream, ",");
                        str key = g->name;
                        TypeDef* td = map_get(generics->generic_type_ctx->resolved, key);
                        fprintf(stream, "%s%p", td->name->name, td);
                    }));
                }));
            }
            if (generics->generic_func_ctx != NULL) {
                fprintf(stream, "@F<%s>", to_str_writer(stream, {
                    list_foreach_i(&generics->generic_func_ctx->generics, lambda(void, usize i, Identifier* g, {
                        if (i > 0) fprintf(stream, ",");
                        str key = g->name;
                        TypeDef* td = map_get(generics->generic_func_ctx->resolved, key);
                        fprintf(stream, "%s%p", td->name->name, td);
                    }));
                }));
            }
        }
    });
}

static void* resolve_item_raw(Program* program, Module* module, Path* path, ModuleItemType kind, str* error) {
    str full_name = to_str_writer(stream, fprint_path(stream, path));
    Identifier* name = path->elements.elements[path->elements.length-1];
    
    path->elements.length -= 1;
    str pathstr = to_str_writer(stream, fprint_path(stream, path));
    path->elements.length += 1;
    
    if (path->absolute) {
        Module* item_module = map_get(program->modules, pathstr);
        if (item_module == NULL) {
            Span* s = gc_malloc(sizeof(Span));
            s->left = path->elements.elements[0]->span.left;
            s->right = name->span.right;
            *error = to_str_writer(stream, fprintf(stream, "no such module %s for item %s of kind %s", pathstr, name->name, ModuleItemType__NAMES[kind]));
            return s;
        }
        ModuleItem* item = map_get(item_module->items, name->name);
        if (item == NULL) {
            *error = to_str_writer(stream, fprintf(stream, "no such item %s of kind %s", full_name, ModuleItemType__NAMES[kind]));
            return &name->span;
        }
        if (item->type != kind) {
            *error = to_str_writer(stream, fprintf(stream, "%s should resolve to %s but got %s @ %s (1)", full_name, ModuleItemType__NAMES[kind], ModuleItemType__NAMES[item->type], to_str_writer(s, fprint_span(s, &item->span))));
            return &name->span;
        }
        return item;
    } else {
        if (path->elements.length > 1) {
            Path* joined = path_join(module->path, path);
            Span joined_span = from_points(&joined->elements.elements[0]->span.left, &joined->elements.elements[0]->span.right);
            str err1 = NULL;
            str err2 = NULL;
            Path* abs = path_new(true, path->elements);
            Span abs_span = from_points(&abs->elements.elements[0]->span.left, &abs->elements.elements[0]->span.right);
            str path_1 = to_str_writer(stream, fprint_path(stream, joined));
            str path_2 = to_str_writer(stream, fprint_path(stream, abs));
            if (str_eq(path_1, path_2)) return resolve_item_raw(program, module, joined, kind, error);
            ModuleItem* candidate_1 = resolve_item_raw(program, module, joined, kind, &err1);
            ModuleItem* candidate_2 = resolve_item_raw(program, module, abs, kind, &err2);
            if (err1 != NULL && err2 != NULL) {
                Span* s = gc_malloc(sizeof(Span));
                s->left = path->elements.elements[0]->span.left;
                s->right = name->span.right;
                *error = to_str_writer(stream, fprintf(stream, "no such item %s of kind %s", full_name, ModuleItemType__NAMES[kind]));
                return s;
            }
            if (err1 == NULL && err2 == NULL) {
                Span* s = gc_malloc(sizeof(Span));
                s->left = path->elements.elements[0]->span.left;
                s->right = name->span.right;
                *error = to_str_writer(stream, fprintf(stream, "Multiple candiates for %s %s: %s %s @ %s and %s %s @ %s", ModuleItemType__NAMES[kind], full_name, 
                            ModuleItemType__NAMES[candidate_1->type], to_str_writer(s, fprint_path(s, joined)), 
                                to_str_writer(s, fprint_span(s, &joined_span)),
                            ModuleItemType__NAMES[candidate_2->type], to_str_writer(s, fprint_path(s, abs)), 
                                to_str_writer(s, fprint_span(s, &abs_span))));
                return s;
            }
            if (err1 == NULL) {
                if (candidate_1->type != kind) {
                    *error = to_str_writer(stream, fprintf(stream, "%s should resolve to %s but got %s @ %s (2a)", path_1, ModuleItemType__NAMES[kind], ModuleItemType__NAMES[candidate_1->type], to_str_writer(s, fprint_span(s, &candidate_1->span))));
                    return &name->span;
                }
                return candidate_1;
            }
            if (err2 == NULL) {
                if (candidate_2->type != kind) {
                    *error = to_str_writer(stream, fprintf(stream, "%s should resolve to %s but got %s @ %s (2b)", path_2, ModuleItemType__NAMES[kind], ModuleItemType__NAMES[candidate_2->type], to_str_writer(s, fprint_span(s, &candidate_2->span))));
                    return &name->span;
                }
                return candidate_2;
            }
        }
        ModuleItem* item = map_get(module->items, name->name);
        if (item == NULL) {
            *error = to_str_writer(stream, fprintf(stream, "no such item %s of kind %s", full_name, ModuleItemType__NAMES[kind]));
            return &name->span;
        }
        if (item->type != kind) {
            *error = to_str_writer(stream, fprintf(stream, "%s should resolve to %s but got %s @ %s (3)", full_name, ModuleItemType__NAMES[kind], ModuleItemType__NAMES[item->type], to_str_writer(s, fprint_span(s, &item->span))));
            return &name->span;
        }
        return item;
    }
}

void register_item(GenericValues* item_values, GenericKeys* gkeys) {
    // no need to register if we are not generic - we just complie the one default nongeneric variant in that case
    if (item_values != NULL) {
        str key = gvals_to_key(item_values);
        if (!map_contains(gkeys->generic_uses, key)) {
            map_put(gkeys->generic_uses, key, item_values);
            list_append(&gkeys->generic_use_keys, key);
        }
    }
}

void resolve_funcdef(Program* program, FuncDef* func, bool* head_resolved);
void resolve_typedef(Program* program, TypeDef* ty, bool* head_resolved);
void resolve_typevalue(Program* program, Module* module, TypeValue* tval, GenericKeys* func_generics, GenericKeys* type_generics);
void* resolve_item(Program* program, Module* module, Path* path, ModuleItemType kind, GenericKeys* func_generics, GenericKeys* type_generics, GenericValues** gvalsref) {
    str error = NULL;
    void* item = resolve_item_raw(program, module, path, kind, &error);
    if (error != NULL) {
        Span span = *(Span*)item;
        spanned_error("Unable to resolve item", span, "%s", error);
    }
    ModuleItem* mi = item;
    if (!mi->head_resolved) {
        switch (mi->type) {
            case MIT_FUNCTION:
                resolve_funcdef(program, mi->item, &mi->head_resolved);
                break;
            case MIT_STRUCT:
                resolve_typedef(program, mi->item, &mi->head_resolved);
                break;
            default:
                unreachable();
        }
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
        gvals = gc_malloc(sizeof(GenericValues));
        gvals->generic_func_ctx = NULL;
        gvals->generic_type_ctx = NULL;
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
        if (gvals->generic_func_ctx == NULL && func_generics != NULL && map_contains(func_generics->resolved, generic_value->def->name->name)) {
            gvals->generic_func_ctx = func_generics;
        }
        if (gvals->generic_type_ctx == NULL && type_generics != NULL && map_contains(type_generics->resolved, generic_value->def->name->name)) {
            gvals->generic_type_ctx = type_generics;
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
        if (func_gen_t != NULL && type_gen_t != NULL) spanned_error("Multiple generic candidates", tval->name->elements.elements[0]->span, "Generic %s is could belong to  %s or %s", 
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
    VarBox* v = gc_malloc(sizeof(VarBox));
    v->name = var->name->name;
    v->id = vars->length;
    v->resolved = gc_malloc(sizeof(TVBox));
    v->resolved->type = NULL;
    var->id = v->id;
    list_append(vars, v);
    return v;
}

TypeValue* replace_generic(TypeValue* tv, GenericValues* ctx) {
    if (ctx == NULL) return tv;
    if (tv->generics != NULL) {
        TypeValue* clone = gc_malloc(sizeof(TypeValue));
        clone->def = tv->def;
        clone->name = tv->name;
        clone->generics = gc_malloc(sizeof(GenericValues));
        clone->generics->generic_func_ctx = tv->generics->generic_func_ctx;
        clone->generics->generic_type_ctx = tv->generics->generic_type_ctx;
        clone->generics->span = tv->generics->span;
        clone->generics->generics = list_new(TypeValueList);
        clone->generics->resolved = map_new();
        Map* rev = map_new();
        map_foreach(tv->generics->resolved, lambda(void, str key, TypeValue* val, {
            map_put(rev, to_str_writer(s, fprintf(s, "%p", val)), key);
        }));
        list_foreach(&tv->generics->generics, lambda(void, TypeValue* val, {
            str key = to_str_writer(stream, fprint_typevalue(stream, val));
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
            for (usize i = 0;i < tv1->generics->generics.length;i++) {
                patch_tvs(&tv1->generics->generics.elements[i], &tv2->generics->generics.elements[i]);
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
                spanned_error("No such generic", match->name->elements.elements[0]->span, "%s is not a valid genericfor %s @ %s", to_str_writer(s, fprint_typevalue(s, match)), 
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
    TVBox* box = gc_malloc(sizeof(TVBox));
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
            resolve_expr(program, func, type_generics, expr->expr, vars, t_return);
            todo("resolve EXPR_BREAK");
        } break;
        case EXPR_CONTINUE: {
            todo("resolve EXPR_CONTINUE -> unit type");
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
                if (str_eq(path, "::std::ptr")) {
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
                spanned_error("Field not initialized", slit->type->name->elements.elements[0]->span, "Field '%s' of struct %s was not initialized", type->name->name, key);
            }));
        
            register_item(slit->type->generics, type->generics);        
        } break;
        case EXPR_C_INTRINSIC: {
            CIntrinsic* ci = expr->expr;
            map_foreach(ci->var_bindings, lambda(void, str key, Variable* var, {
                VarBox* vb = var_find(vars, var);
                var->id = vb->id;
            }));
            map_foreach(ci->type_bindings, lambda(void, str key, TypeValue* tv, {
                resolve_typevalue(program, func->module, tv, func->generics, type_generics);
            }));
            resolve_typevalue(program, func->module, ci->ret_ty, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, ci->ret_ty);
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            TVBox* inner_tv = new_tvbox();
            if (t_return->type != NULL) {
                TypeValue* reference = gen_typevalue("::std::ptr<_>", &expr->span);
                resolve_typevalue(program, func->module, reference, func->generics, type_generics);
                if (reference->def != t_return->type->def) spanned_error("Expected resulting type to be a reference", expr->span, "Type %s, is not a reference, expected %s as a result", to_str_writer(s, fprint_typevalue(s, t_return->type)),to_str_writer(s, fprint_typevalue(s, reference)));
                inner_tv->type = t_return->type->generics->generics.elements[0];
            }
            resolve_expr(program, func, type_generics, inner, vars, inner_tv);
            finish_tvbox(inner_tv);
            TypeValue* reference = gen_typevalue("::std::ptr::<_>", &expr->span);
            reference->generics->generics.elements[0] = inner->resolved->type;
            resolve_typevalue(program, func->module, reference, func->generics, type_generics);
            fill_tvbox(program, func->module, expr->span, func->generics, type_generics, t_return, reference);
        } break;
        case EXPR_DEREF: {
            Expression* inner = expr->expr;
            TVBox* inner_tv = new_tvbox();
            if (t_return->type != NULL) {
                inner_tv->type = gen_typevalue("::std::ptr::<_>", &expr->span);
                inner_tv->type->generics->generics.elements[0] = t_return->type;
            }
            resolve_expr(program, func, type_generics, inner, vars, inner_tv);
            finish_tvbox(inner_tv);
            if (str_eq(to_str_writer(s, fprint_path(s, inner_tv->type->name)), "::std::ptr")) spanned_error("Expected ptr to dereference", expr->span, "Cannot dereference type %s, expected ::std::ptr<_>", to_str_writer(s, fprint_typevalue(s, inner->resolved->type)));
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
        yield_ty = gc_malloc(sizeof(TVBox));
        yield_ty->type = gen_typevalue("::std::unit", &block->span);
        resolve_typevalue(program, func->module, yield_ty->type, func->generics, type_generics);
    }
    block->res = yield_ty->type;
    fill_tvbox(program, func->module, block->span, func->generics, type_generics, t_return, yield_ty->type);
    
    finish_tvbox(yield_ty);
    
    vars->length = restore_len;
}

void resolve_module(Program* program, Module* module);
void resolve_funcdef(Program* program, FuncDef* func, bool* head_resolved) {
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
            TypeDef* type = gc_malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = NULL;
            type->module = NULL;
            map_put(func->generics->resolved, key->name, type);
        }));
    }
    GenericKeys* type_generics = NULL;
    VarList vars = list_new(VarList);
    list_foreach(&func->args, lambda(void, Argument* arg, {
        VarBox* v = var_register(&vars, arg->var);
        v->resolved->type = arg->type;
        resolve_typevalue(program, func->module, v->resolved->type, func->generics, type_generics);
    }));
    resolve_typevalue(program, func->module, func->return_type, func->generics, type_generics);

    *head_resolved = true;

    if (func->body != NULL) {
        TVBox* blockbox = new_tvbox();
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

void resolve_typedef(Program* program, TypeDef* ty, bool* head_resolved) {
    if (!ty->module->resolved && !ty->module->in_resolution) {
        resolve_module(program, ty->module);
    }
    if (ty->extern_ref != NULL) {
        log("Type %s is extern", ty->name->name);
        *head_resolved = true;
        return;
    }
    log("Resolving type %s", ty->name->name);
    if (str_eq(ty->name->name, "_")) spanned_error("Invalid type name", ty->name->span, "`_` is a reserved name.");
    if (ty->generics != NULL) {
        list_foreach(&ty->generics->generics, lambda(void, Identifier* key, {
            TypeDef* type = gc_malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = NULL;
            type->module = NULL;
            map_put(ty->generics->resolved, key->name, type);
        }));
    }
    *head_resolved = true;
    map_foreach(ty->fields, lambda(void, str name, Field* field, {
        resolve_typevalue(program, ty->module, field->type, NULL, ty->generics);
    }));
}

void resolve_module(Program* program, Module* module) {
    if (module->resolved) return;
    str path_str = to_str_writer(stream, fprint_path(stream, module->path));
    if (module->in_resolution) panic("recursion detected while resolving %s", path_str);
    module->in_resolution = true;

    log("Resolving module %s", path_str);

    list_foreach(&module->imports, lambda(void, Path* path, {
        Module* mod = map_get(program->modules, to_str_writer(stream, fprint_path(stream, path)));
        resolve_module(program, mod);
    }));

    map_foreach(module->items, lambda(void, str key, ModuleItem* item, {
        if (item->head_resolved) return; // this means at this point the whole item is already resolved, the "head" part is for recursion detection
        switch (item->type) {
            case MIT_FUNCTION:
                resolve_funcdef(program, item->item, &item->head_resolved);
                break;
            case MIT_STRUCT:
                resolve_typedef(program, item->item, &item->head_resolved);
                break;
        }
    }));

    module->resolved = true;
    module->in_resolution = false;
}

void resolve(Program* program) {
    map_foreach(program->modules, lambda(void, str key, Module* mod, {
        resolve_module(program, mod);
    }));
}