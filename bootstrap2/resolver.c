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

static void* resolve_item_raw(Program* program, Module* module, Path* path, ModuleItemType kind, str* error, Module** out_owning) {
    *out_owning = NULL;
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
        *out_owning = item_module;
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
            if (str_eq(path_1, path_2)) return resolve_item_raw(program, module, joined, kind, error, out_owning);
            Module* out_owning_1 = NULL;
            Module* out_owning_2 = NULL;
            ModuleItem* candidate_1 = resolve_item_raw(program, module, joined, kind, &err1, &out_owning_1);
            ModuleItem* candidate_2 = resolve_item_raw(program, module, abs, kind, &err2, &out_owning_2);
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
                *out_owning = out_owning_1;
                return candidate_1;
            }
            if (err2 == NULL) {
                if (candidate_2->type != kind) {
                    *error = to_str_writer(stream, fprintf(stream, "%s should resolve to %s but got %s @ %s (2b)", path_2, ModuleItemType__NAMES[kind], ModuleItemType__NAMES[candidate_2->type], to_str_writer(s, fprint_span(s, &candidate_2->span))));
                    return &name->span;
                }
                *out_owning = out_owning_2;
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
        *out_owning = module;
        return item;
    }
}

void resolve_funcdef(Program* program, FuncDef* func, bool* head_resolved);
void resolve_typedef(Program* program, TypeDef* ty, bool* head_resolved);
void resolve_typevalue(Program* program, Module* module, TypeValue* tval, GenericKeys* func_generics, GenericKeys* type_generics);
void* resolve_item(Program* program, Module* module, Path* path, ModuleItemType kind, GenericKeys* func_generics, GenericKeys* type_generics, GenericValues* gvals) {
    Module* item_module = NULL;
    str error = NULL;
    void* item = resolve_item_raw(program, module, path, kind, &error, &item_module);
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
    Span span = from_points(&path->elements.elements[0]->span.left, &path->elements.elements[path->elements.length-1]->span.right);
    if ((gkeys == NULL || gkeys->generics.length != 0) && (gvals == NULL || gvals->generics.length == 0)) goto generic_end;
    if (gkeys == NULL && gvals != NULL) spanned_error("Unexpected generics", span, "Expected no generics for %s, got %llu", fullname, gvals->generics.length);
    if (gkeys != NULL && gvals == NULL) spanned_error("Missing generics", span, "Expected %llu for %s, got none", gkeys->generics.length, fullname);
    if (gkeys->generics.length != gvals->generics.length) spanned_error("Generics mismatch", span, "Expected %llu for %s, got %llu", gkeys->generics.length, gvals->generics.length);
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
    // no need to register if we are not generic - we just complie the one default nongeneric variant in that case
    if (gkeys != NULL) {
        str key = gvals_to_key(gvals);
        if (!map_contains(gkeys->generic_uses, key)) {
            map_put(gkeys->generic_uses, key, gvals);
            list_append(&gkeys->generic_use_keys, key);
        }
    }
    return mi->item;
}

void resolve_typevalue(Program* program, Module* module, TypeValue* tval, GenericKeys* func_generics, GenericKeys* type_generics) {
    if (tval->def != NULL) return;
    // might be a generic? >`OvO´<
    if (!tval->name->absolute && tval->name->elements.length == 1) {
        str key = tval->name->elements.elements[0]->name;
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
    TypeDef* td = resolve_item(program, module, tval->name, MIT_STRUCT, func_generics, type_generics, tval->generics);
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
    if (tv1->def != tv2->def) {
        Span span1 = from_points(&tv1->name->elements.elements[0]->span.left, &tv1->name->elements.elements[tv1->name->elements.length-1]->span.right);
        Span span2 = from_points(&tv2->name->elements.elements[0]->span.left, &tv2->name->elements.elements[tv2->name->elements.length-1]->span.right);
        spanned_error("Types do not match", span, "%s @ %s and %s @ %s", 
            to_str_writer(stream, fprint_typevalue(stream, tv1)), to_str_writer(stream, fprint_span(stream, &span1)),
            to_str_writer(stream, fprint_typevalue(stream, tv2)), to_str_writer(stream, fprint_span(stream, &span2))
        );
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

void resolve_block(Program* program, FuncDef* func, GenericKeys* type_generics, Block* block, VarList* vars);
void resolve_expr(Program* program, FuncDef* func, GenericKeys* type_generics, Expression* expr, VarList* vars) {
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            resolve_expr(program, func, type_generics, op->lhs, vars);
            resolve_expr(program, func, type_generics, op->rhs, vars);
            assert_types_equal(program, func->module, op->lhs->resolved->type, op->rhs->resolved->type, op->op_span, func->generics, type_generics);
            if (str_eq(op->op, ">") || str_eq(op->op, "<") || str_eq(op->op, ">=") || str_eq(op->op, "<=") || str_eq(op->op, "!=") || str_eq(op->op, "==")) {
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = gen_typevalue("::std::bool", &op->op_span);
                resolve_typevalue(program, func->module, expr->resolved->type, func->generics, type_generics);
            } else {
                expr->resolved = op->lhs->resolved;
            }
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncDef* fd = resolve_item(program, func->module, fc->name, MIT_FUNCTION, func->generics, type_generics, fc->generics);
            fc->def = fd;
            if (fd->is_variadic) {
                if (fd->args.length > fc->arguments.length) spanned_error("Too few args for variadic function", expr->span, "expected at least %llu args, got %llu", fd->args.length, fc->arguments.length);
            } else {
                if (fd->args.length != fc->arguments.length) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", fd->args.length, fc->arguments.length);
            }
            list_foreach_i(&fc->arguments, lambda(void, usize i, Expression* arg, {
                resolve_expr(program, func, type_generics, arg, vars);
                if (i >= fd->args.length) return;
                TypeValue* arg_tv = replace_generic(fd->args.elements[i]->type, fc->generics);
                assert_types_equal(program, func->module, arg->resolved->type, arg_tv, arg->span, func->generics, type_generics);
            }));
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = replace_generic(fd->return_type, fc->generics);
            //expr->resolved->type = fd->return_type;
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING: {
                    expr->resolved = gc_malloc(sizeof(TVBox));
                    expr->resolved->type = gen_typevalue("::std::c_const_str_ptr", &expr->span);
                    resolve_typevalue(program, func->module, expr->resolved->type, func->generics, type_generics);
                } break;
                case NUMERAL: {
                    str num = lit->string;
                    str ty = "i32";
                    for (usize i = 0;i < strlen(num);i++) {
                        if (num[i] == 'u' || num[i] == 'i') { ty = num + i;  break; }
                    }
                    expr->resolved = gc_malloc(sizeof(TVBox));
                    expr->resolved->type = gen_typevalue(to_str_writer(stream, fprintf(stream, "::std::%s", ty)), &expr->span);
                    resolve_typevalue(program, func->module, expr->resolved->type, func->generics, type_generics);
                } break; 
                default:
                    unreachable("invalid literal type %s", TokenType__NAMES[lit->type]);
            }
        } break;
        case EXPR_BLOCK: {
            Block* block = expr->expr;
            resolve_block(program, func, type_generics, block, vars);
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = block->res;
        } break;
        case EXPR_VARIABLE: {
            Variable* var = expr->expr;
            VarBox* vb = var_find(vars, var);
            var->id = vb->id;
            expr->resolved = vb->resolved;
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;

            resolve_expr(program, func, type_generics, let->value, vars);
            VarBox* v = var_register(vars, let->var);
            let->var->id = v->id;
            v->resolved = let->value->resolved;
            if (let->type == NULL) {
                let->type = let->value->resolved->type;
            } else {
                resolve_typevalue(program, func->module, let->type, func->generics, type_generics);
                assert_types_equal(program, func->module, let->type, let->value->resolved->type, expr->span, func->generics, type_generics);
            }

            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = gen_typevalue("::std::unit", &expr->span);
            resolve_typevalue(program, func->module, expr->resolved->type, func->generics, type_generics);
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            resolve_expr(program, func, type_generics, assign->asignee, vars);
            resolve_expr(program, func, type_generics, assign->value, vars);
            assert_types_equal(program, func->module, assign->asignee->resolved->type, assign->value->resolved->type, expr->span, func->generics, type_generics);
            expr->resolved = assign->value->resolved;
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            resolve_expr(program, func, type_generics, cond->cond, vars);
            TypeValue* bool_ty = gen_typevalue("::std::bool", &cond->cond->span);
            resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
            assert_types_equal(program, func->module, cond->cond->resolved->type, bool_ty, cond->cond->span, func->generics, type_generics);
            resolve_block(program, func, type_generics, cond->then, vars);
            if (cond->otherwise != NULL) {
                resolve_block(program, func, type_generics, cond->otherwise, vars);
                assert_types_equal(program, func->module, cond->then->res, cond->otherwise->res, expr->span, func->generics, type_generics);
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = cond->then->res;
            } else{
                TypeValue* unit_ty = gen_typevalue("::std::unit", &cond->cond->span);
                resolve_typevalue(program, func->module, unit_ty, func->generics, type_generics);
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = cond->then->res;
                assert_types_equal(program, func->module, cond->then->res, unit_ty, expr->span, func->generics, type_generics);
            }
        } break;
        case EXPR_WHILE_LOOP: {
            WhileLoop* wl = expr->expr;
            resolve_expr(program, func, type_generics, wl->cond, vars);
            TypeValue* bool_ty = gen_typevalue("::std::bool", &wl->cond->span);
            resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
            assert_types_equal(program, func->module, wl->cond->resolved->type, bool_ty, wl->cond->span, func->generics, type_generics);
            resolve_block(program, func, type_generics, wl->body, vars);
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = wl->body->res;
        } break;
        case EXPR_RETURN: {
            Expression* ret = expr->expr;
            if (expr == NULL) {
                TypeValue* bool_ty = gen_typevalue("::std::unit", &expr->span);
                resolve_typevalue(program, func->module, bool_ty, func->generics, type_generics);
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = bool_ty;
            } else {
                resolve_expr(program, func, type_generics, ret, vars);
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = ret->resolved->type;
            }
            assert_types_equal(program, func->module, expr->resolved->type, func->return_type, func->name->span, func->generics, type_generics);
        } break;
        case EXPR_BREAK: {
            resolve_expr(program, func, type_generics, expr->expr, vars);
            todo("resolve EXPR_BREAK");
        } break;
        case EXPR_CONTINUE: {
            todo("resolve EXPR_CONTINUE -> unit type");
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            resolve_expr(program, func, type_generics, fa->object, vars);
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
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = field_ty;
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            resolve_typevalue(program, func->module, slit->type, func->generics, type_generics);
            TypeDef* type = slit->type->def;
            Map* temp_fields = map_new();
            map_foreach(type->fields, lambda(void, str key, Field* field, {
                map_put(temp_fields, key, field);
            }));
            map_foreach(slit->fields, lambda(void, str key, StructFieldLit* field, {
                Field* f = map_remove(temp_fields, key);
                if (f == NULL) spanned_error("Invalid struct field", field->name->span, "Struct %s has no such field '%s'", type->name->name, key);
                resolve_expr(program, func, type_generics, field->value, vars);
                TypeValue* value_ty = field->value->resolved->type;
                TypeValue* field_ty = replace_generic(f->type, slit->type->generics);
                assert_types_equal(program, func->module, value_ty, field_ty, field->name->span, func->generics, type_generics);
            }));
            map_foreach(temp_fields, lambda(void, str key, Field* field, {
                spanned_error("Field not initialized", slit->type->name->elements.elements[0]->span, "Field '%s' of struct %s was not initialized", type->name->name, key);
            }));
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = slit->type;
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
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = ci->ret_ty;
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            resolve_expr(program, func, type_generics, inner, vars);
            TypeValue* reference = gen_typevalue("::std::ptr::<T>", &expr->span);
            reference->generics->generics.elements[0] = inner->resolved->type;
            resolve_typevalue(program, func->module, reference, func->generics, type_generics);
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = reference;
        } break;
        case EXPR_DEREF: {
            Expression* inner = expr->expr;
            resolve_expr(program, func, type_generics, inner, vars);
            TypeValue* reference = gen_typevalue("::std::ptr", &expr->span);
            resolve_typevalue(program, func->module, reference, func->generics, type_generics);
            if (reference->def != inner->resolved->type->def) spanned_error("Expected ptr to dereference", expr->span, "Cannot dereference type %s, expected %s", to_str_writer(s, fprint_typevalue(s, inner->resolved->type)),to_str_writer(s, fprint_typevalue(s, reference)));
            if (inner->resolved->type->generics == NULL || inner->resolved->type->generics->generics.length != 1) spanned_error("Expected ptr to have a pointee", expr->span, "Pointer %s should have one generic argument as its pointee", to_str_writer(s, fprint_typevalue(s, inner->resolved->type)));
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = inner->resolved->type->generics->generics.elements[0];
        } break;
        default:
            unreachable("%s", ExprType__NAMES[expr->type]);
    }
}

void resolve_block(Program* program, FuncDef* func, GenericKeys* type_generics, Block* block, VarList* vars) {
    usize restore_len = vars->length;
    
    list_foreach(&block->expressions, lambda(void, Expression* expr, {
        resolve_expr(program, func, type_generics, expr, vars);
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

    vars->length = restore_len;
}

void resolve_module(Program* program, Module* module);
void resolve_funcdef(Program* program, FuncDef* func, bool* head_resolved) {
    if (!func->module->resolved && !func->module->in_resolution) {
        resolve_module(program, func->module);
    }
    log("Resolving function %s", func->name->name);
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
        resolve_block(program, func, type_generics, func->body, &vars);
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
