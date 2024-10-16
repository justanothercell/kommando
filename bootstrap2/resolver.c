#include "ast.h"
#include "lib.h"
#include "lib/debug.h"
#include "lib/defines.h"
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

void* try_resolve_item(Program* program, Module* module, Path* path, ModuleItemType kind, str* error, Module** out_owning) {
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
            if (str_eq(path_1, path_2)) return try_resolve_item(program, module, joined, kind, error, out_owning);
            Module* out_owning_1 = NULL;
            Module* out_owning_2 = NULL;
            ModuleItem* candidate_1 = try_resolve_item(program, module, joined, kind, &err1, &out_owning_1);
            ModuleItem* candidate_2 = try_resolve_item(program, module, abs, kind, &err2, &out_owning_2);
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

void* resolve_item_module(Program* program, Module* module, Path* path, ModuleItemType kind, Module** out_owning) {
    str error = NULL;
    void* item = try_resolve_item(program, module, path, kind, &error, out_owning);
    if (error == NULL) return ((ModuleItem*)item)->item;
    Span span = *(Span*)item;
    spanned_error("Unable to resolve item", span, "%s", error);
}
void* resolve_item(Program* program, Module* module, Path* path, ModuleItemType kind) {
    Module* out_owning = NULL;
    resolve_item_module(program, module, path, kind, &out_owning);
}

void resolve_typevalue(Program* program, Module* module, TypeValue* tval, Map* generics) {
    Module* out_owning = NULL;
    if (tval->def != NULL) return;
    if (generics != NULL && !tval->name->absolute && tval->name->elements.length == 1) {
        str key = tval->name->elements.elements[0]->name;
        TypeDef* type = map_get(generics, key);
        if (type != NULL) {
            tval->def = type;
            return;
        }
    }
    tval->def = resolve_item_module(program, module, tval->name, MIT_STRUCT, &out_owning);
}

static void fprint_fu_res_tv(FILE* stream, TypeValue* tv) {
    fprintf(stream, "%s%p", tv->def->name->name, tv->def);
    if (tv->generics != NULL && tv->generics->generics.length > 0) {
        fprintf(stream, "<");
        list_foreach_i(&tv->generics->generics, lambda(void, (usize i, TypeValue* tv) {
            fprint_fu_res_tv(stream, tv);
            fprintf(stream, ",");
        }));
        fprintf(stream, ">");
    }
}

str funcusage_to_key(FuncUsage* fu) {
    return to_str_writer(stream, {
        fprintf(stream, "#");
        if (fu->generics != NULL && map_size(fu->generics) > 0) {
            fprintf(stream, "<");
            map_foreach(fu->generics, lambda(void, (str  _key, TypeValue* tv) {
                fprint_fu_res_tv(stream, tv);
                fprintf(stream, ",");
            }));
            fprintf(stream, ">");
        }
        if (fu->generic_use != NULL) {
            fprintf(stream, "@%s%p", fu->generic_use->name->name, fu->generic_use);
        }
    });
}

void assert_types_equal(Program* program, Module* module, TypeValue* tv1, TypeValue* tv2, Span span, Map* generics) {
    if (tv1->def == NULL) resolve_typevalue(program, module, tv1, generics);
    if (tv2->def == NULL) resolve_typevalue(program, module, tv2, generics);
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
void resolve_funcdef(Program* program, Module* module, FuncDef* func);
void resolve_block(Program* program, Module* module, FuncDef* func, Block* block, VarList* vars);
void resolve_expr(Program* program, Module* module, FuncDef* func, Expression* expr, VarList* vars) {
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            resolve_expr(program, module, func, op->lhs, vars);
            resolve_expr(program, module, func, op->rhs, vars);
            assert_types_equal(program, module, op->lhs->resolved->type, op->rhs->resolved->type, op->op_span, func->resolved_generics);
            if (str_eq(op->op, ">") || str_eq(op->op, "<") || str_eq(op->op, ">=") || str_eq(op->op, "<=") || str_eq(op->op, "!=") || str_eq(op->op, "==")) {
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = gen_typevalue("::std::bool", &op->op_span);
                resolve_typevalue(program, module, expr->resolved->type, func->resolved_generics);
            } else {
                expr->resolved = op->lhs->resolved;
            }
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            Module* fd_module;
            FuncDef* fd = resolve_item_module(program, module, fc->name, MIT_FUNCTION, &fd_module);
            if (!fd->head_resolved) resolve_funcdef(program, fd_module, fd);
            resolve_typevalue(program, module, fd->return_type, fd->resolved_generics);
            FuncUsage* fu = gc_malloc(sizeof(FuncUsage));
            fu->generics = map_new();
            fu->generic_use = NULL;
            if (fc->generics != NULL && fd->generics == NULL) spanned_error("Function is not generic", expr->span, "This function is not defined with generic arguemnts");
            if (fc->generics != NULL) {
                if (fc->generics->generics.length != fd->generics->generics.length) spanned_error("Generic count mismatch", expr->span, "expected %llu, got %llu generic args", fd->generics->generics.length, fc->generics->generics.length);
                list_foreach_i(&fc->generics->generics, lambda(void, (usize i, TypeValue* generic) {
                    resolve_typevalue(program, module, generic, func->resolved_generics);
                    map_put(fu->generics, fd->generics->generics.elements[i]->name, generic);
                    // generic function called inside a generic functions with the outer funcs generic args applied to inner, leading to an indirect resolve
                    if (func->resolved_generics != NULL && map_contains(func->resolved_generics, generic->def->name->name)) {
                        if (fu->generic_use == NULL) {
                            fu->generic_use = func;
                        }
                    }
                }));
            }
            fc->def = fd;
            fc->fu = fu;
            if (fd->is_variadic) {
                if (fd->args.length > fc->arguments.length) spanned_error("Too few args for variadic function", expr->span, "expected at least %llu args, got %llu", fd->args.length, fc->arguments.length);
            } else {
                if (fd->args.length != fc->arguments.length) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", fd->args.length, fc->arguments.length);
            }
            list_foreach_i(&fc->arguments, lambda(void, (usize i, Expression* arg) {
                resolve_expr(program, module, func, arg, vars);
                if (i >= fd->args.length) return;
                TypeValue* arg_tv = fd->args.elements[i]->type;
                if (fu->generics != NULL && !arg_tv->name->absolute && arg_tv->name->elements.length == 1 && map_contains(fu->generics, arg_tv->name->elements.elements[0]->name)) {
                    arg_tv = map_get(fu->generics, arg_tv->name->elements.elements[0]->name);
                }
                assert_types_equal(program, module, arg->resolved->type, arg_tv, arg->span, fu->generics);
            }));
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = fd->return_type;
            if (fu->generics != NULL && !fd->return_type->name->absolute && fd->return_type->name->elements.length == 1 && map_contains(fu->generics, fd->return_type->name->elements.elements[0]->name)) {
                expr->resolved->type =  map_get(fu->generics, fd->return_type->name->elements.elements[0]->name);
            }
            str fukey = funcusage_to_key(fu);
            if (!map_contains(fd->generic_uses, fukey)) {
                map_put(fd->generic_uses, fukey, fu);
            }
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING: {
                    expr->resolved = gc_malloc(sizeof(TVBox));
                    expr->resolved->type = gen_typevalue("::std::c_const_str_ptr", &expr->span);
                    resolve_typevalue(program, module, expr->resolved->type, func->resolved_generics);
                } break;
                case NUMERAL: {
                    str num = lit->string;
                    str ty = "i32";
                    for (usize i = 0;i < strlen(num);i++) {
                        if (num[i] == 'u' || num[i] == 'i') { ty = num + i;  break; }
                    }
                    expr->resolved = gc_malloc(sizeof(TVBox));
                    expr->resolved->type = gen_typevalue(to_str_writer(stream, fprintf(stream, "::std::%s", ty)), &expr->span);
                    resolve_typevalue(program, module, expr->resolved->type, func->resolved_generics);
                } break; 
                default:
                    unreachable("invalid literal type %s", TokenType__NAMES[lit->type]);
            }
        } break;
        case EXPR_BLOCK: {
            Block* block = expr->expr;
            resolve_block(program, module, func, block, vars);
            todo("resolve EXPR_BLOCK");
        } break;
        case EXPR_VARIABLE: {
            Variable* var = expr->expr;
            VarBox* vb = var_find(vars, var);
            var->id = vb->id;
            expr->resolved = vb->resolved;
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;

            VarBox* v = var_register(vars, let->var);
            let->var->id = v->id;
            resolve_expr(program, module, func, let->value, vars);
            v->resolved = let->value->resolved;
            if (let->type == NULL) {
                let->type = let->value->resolved->type;
            } else {
                resolve_typevalue(program, module, let->type, func->resolved_generics);
                assert_types_equal(program, module, let->type, let->value->resolved->type, expr->span, func->resolved_generics);
            }

            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = gen_typevalue("::std::unit", &expr->span);
            resolve_typevalue(program, module, expr->resolved->type, func->resolved_generics);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            resolve_expr(program, module, func, cond->cond, vars);
            TypeValue* bool_ty = gen_typevalue("::std::bool", &cond->cond->span);
            resolve_typevalue(program, module, bool_ty, func->resolved_generics);
            assert_types_equal(program, module, cond->cond->resolved->type, bool_ty, cond->cond->span, func->resolved_generics);
            resolve_block(program, module, func, cond->then, vars);
            if (cond->otherwise != NULL) {
                resolve_block(program, module, func, cond->otherwise, vars);
                assert_types_equal(program, module, cond->then->res, cond->otherwise->res, expr->span, func->resolved_generics);
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = cond->then->res;
            } else{
                TypeValue* unit_ty = gen_typevalue("::std::unit", &cond->cond->span);
                resolve_typevalue(program, module, unit_ty, func->resolved_generics);
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = cond->then->res;
                assert_types_equal(program, module, cond->then->res, unit_ty, expr->span, func->resolved_generics);
            }
        } break;
        case EXPR_WHILE_LOOP: {
            WhileLoop* wl = expr->expr;
            resolve_expr(program, module, func, wl->cond, vars);
            resolve_block(program, module, func, wl->body, vars);
            todo("resolve EXPR_WHILE_LOOP");
        } break;
        case EXPR_RETURN: {
            resolve_expr(program, module, func, expr->expr, vars);
            todo("resolve EXPR_RETURN");
        } break;
        case EXPR_BREAK: {
            resolve_expr(program, module, func, expr->expr, vars);
            todo("resolve EXPR_BREAK");
        } break;
        case EXPR_CONTINUE: {
            todo("resolve EXPR_CONTINUE -> unit type");
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            resolve_expr(program, module, func, fa->object, vars);
            TypeDef* td = fa->object->resolved->type->def;
            Field* field = map_get(td->fields, fa->field->name);
            if (field == NULL) spanned_error("Invalid struct field", fa->field->span, "Struct %s has no such field '%s'", td->name->name, fa->field->name);
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = field->type;
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            resolve_typevalue(program, module, slit->type, func->resolved_generics);
            TypeDef* type = slit->type->def;
            Map* temp_fields = map_new();
            map_foreach(type->fields, lambda(void, (str key, Field* field) {
                map_put(temp_fields, key, field);
            }));
            map_foreach(slit->fields, lambda(void, (str key, StructFieldLit* field) {
                Field* f = map_remove(temp_fields, key);
                if (f == NULL) spanned_error("Invalid struct field", field->name->span, "Struct %s has no such field '%s'", type->name->name, key);
                resolve_expr(program, module, func, field->value, vars);
                assert_types_equal(program, module, field->value->resolved->type, f->type, field->name->span, func->resolved_generics);
            }));
            map_foreach(temp_fields, lambda(void, (str key, Field* field) {
                spanned_error("Field not initialized", slit->type->name->elements.elements[0]->span, "Field '%s' of struct %s was not initialized", type->name->name, key);
            }));
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = slit->type;
        } break;
        case EXPR_C_INTRINSIC: {
            CIntrinsic* ci = expr->expr;
            map_foreach(ci->var_bindings, lambda(void, (str key, Variable* var) {
                VarBox* vb = var_find(vars, var);
                var->id = vb->id;
            }));
            map_foreach(ci->type_bindings, lambda(void, (str key, TypeValue* tv) {
                resolve_typevalue(program, module, tv, func->resolved_generics);
            }));
            resolve_typevalue(program, module, ci->ret_ty, func->resolved_generics);
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = ci->ret_ty;
        } break;
    }
}

void resolve_block(Program* program, Module* module, FuncDef* func, Block* block, VarList* vars) {
    usize restore_len = vars->length;
    
    list_foreach(&block->expressions, lambda(void, (Expression* expr) {
        resolve_expr(program, module, func, expr, vars);
    }));
    TVBox* yield_ty;
    if (block->yield_last && block->expressions.length > 0) {
        yield_ty = block->expressions.elements[block->expressions.length-1]->resolved;
    } else {
        yield_ty = gc_malloc(sizeof(TVBox));
        yield_ty->type = gen_typevalue("::std::unit", &block->span);
        resolve_typevalue(program, module, yield_ty->type, func->resolved_generics);
    }
    block->res = yield_ty->type;

    vars->length = restore_len;
}

void resolve_module(Program* program, Module* module);
void resolve_funcdef(Program* program, Module* module, FuncDef* func) {
    if (!module->resolved && !module->in_resolution) {
        resolve_module(program, module);
    }
    if (func->head_resolved) return; // this also means that the rest of the functio has been resolved! The "head" part is only to prevent recursion.
    log("Resolving function %s", func->name->name);
    if (func->return_type == NULL) {
        func->return_type = gen_typevalue("::std::unit", &func->name->span);
    }
    func->resolved_generics = NULL;
    if (func->generics != NULL) {
        func->resolved_generics = map_new();
        list_foreach(&func->generics->generics, lambda(void, (Identifier* key) {
            TypeDef* type = gc_malloc(sizeof(TypeDef));
            type->generics = NULL;
            type->name = key;
            type->extern_ref = NULL;
            type->fields = NULL;
            map_put(func->resolved_generics, key->name, type);
        }));
    }
    resolve_typevalue(program, module, func->return_type, func->resolved_generics);
    func->head_resolved = true;
    if (func->body != NULL) {
        VarList vars = list_new(VarList);
        list_foreach(&func->args, lambda(void, (Argument* arg) {
            VarBox* v = var_register(&vars, arg->var);
            v->resolved->type = arg->type;
            resolve_typevalue(program, module, v->resolved->type, func->resolved_generics);
        }));
        resolve_block(program, module, func, func->body, &vars);
        assert_types_equal(program, module, func->body->res, func->return_type, func->name->span, func->resolved_generics);
    }
}

void resolve_module(Program* program, Module* module) {
    if (module->resolved) return;
    str path_str = to_str_writer(stream, fprint_path(stream, module->path));
    if (module->in_resolution) panic("recursion detected while resolving %s", path_str);
    module->in_resolution = true;

    log("Resolving module %s", path_str);

    list_foreach(&module->imports, lambda(void, (Path* path){
        Module* mod = map_get(program->modules, to_str_writer(stream, fprint_path(stream, path)));
        resolve_module(program, mod);
    }));

    map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            resolve_funcdef(program, module, item->item);
            break;
        case MIT_STRUCT:
            break;
        }
    }));

    module->resolved = true;
    module->in_resolution = false;
}

void resolve(Program* program) {
    map_foreach(program->modules, lambda(void, (str key, Module* mod) {
        resolve_module(program, mod);
    }));
}
