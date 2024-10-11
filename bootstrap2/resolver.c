#include "ast.h"
#include "lib.h"
#include "lib/debug.h"
#include "lib/defines.h"
#include "lib/gc.h"
#include "lib/map.h"
#include "lib/str.h"
#include "module.h"
#include "parser.h"
#include "token.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void* resolve_item(Program* program, Module* module, Path* path, ModuleItemType kind) {
    str full_name = to_str_writer(stream, fprint_path(stream, path));
    Identifier* name = path->elements.elements[path->elements.length-1];
    path->elements.length -= 1;
    str pathstr = to_str_writer(stream, fprint_path(stream, path));
    void* ret_item;

    if (path->absolute) {
        Module* item_module = map_get(program->modules, pathstr);
        Span s;
        s.left = path->elements.elements[0]->span.left;
        s.right = name->span.right;
        if (item_module == NULL) spanned_error("Unable to resolve item", s, "no such module %s for item %s of kind %s", pathstr, name->name, ModuleItemType__NAMES[kind]);
        ModuleItem* item = map_get(item_module->items, name->name);
        if (item == NULL) spanned_error("Unable to resolve item", name->span, "no such item %s of kind %s", full_name, ModuleItemType__NAMES[kind]);
        if (item->type != kind) spanned_error("Wrong item kind", s, "%s should resolve to %s but got %s", full_name, ModuleItemType__NAMES[kind], ModuleItemType__NAMES[item->type]);
        ret_item = item->item;
    } else {
        if (path->elements.length > 0) spanned_error("Unable to resolve item", name->span, "relative paths may onöy be of this module");
        ModuleItem* item = map_get(module->items, name->name);
        if (item == NULL) spanned_error("Unable to resolve item", name->span, "no such item %s of kind %s", full_name, ModuleItemType__NAMES[kind]);
        if (item->type != kind) spanned_error("Wrong item kind", name->span, "%s should resolve to %s but got %s", full_name, ModuleItemType__NAMES[kind], ModuleItemType__NAMES[item->type]);
        ret_item = item->item;
    }
    
    path->elements.length += 1;
    return ret_item;
}
void resolve_typevalue(Program* program, Module* module, TypeValue* tval) {
    if (tval->def != NULL) return;
    tval->def = resolve_item(program, module, tval->name, MIT_STRUCT);
}


void assert_types_equal(Program* program, Module* module, TypeValue* tv1, TypeValue* tv2, Span span) {
    if (tv1->def == NULL) resolve_typevalue(program, module, tv1);
    if (tv2->def == NULL) resolve_typevalue(program, module, tv2);
    if (tv1->def != tv2->def) {
        Span span1 = from_points(&tv1->name->elements.elements[0]->span.left, &tv1->name->elements.elements[tv2->name->elements.length-1]->span.right);
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
void resolve_block(Program* program, Module* module, FuncDef* func, Block* block, VarList* vars);
void resolve_expr(Program* program, Module* module, FuncDef* func, Expression* expr, VarList* vars) {
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            resolve_expr(program, module, func, op->lhs, vars);
            resolve_expr(program, module, func, op->rhs, vars);
            assert_types_equal(program, module, op->lhs->resolved->type, op->rhs->resolved->type, op->op_span);
            if (str_eq(op->op, ">") || str_eq(op->op, "<") || str_eq(op->op, ">=") || str_eq(op->op, "<=") || str_eq(op->op, "!=") || str_eq(op->op, "==")) {
                expr->resolved = gc_malloc(sizeof(TVBox));
                expr->resolved->type = gen_typevalue("::std::bool", &op->op_span);
                resolve_typevalue(program, module, expr->resolved->type);
            } else {
                expr->resolved = op->lhs->resolved;
            }
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncDef* fd = resolve_item(program, module, fc->name, MIT_FUNCTION);
            resolve_typevalue(program, module, fd->return_type);
            FuncUsage* fu = gc_malloc(sizeof(FuncUsage));
            fu->context = func;
            fu->generics = map_new();
            str fukey = funcusage_to_key(fu);
            if (!map_contains(fd->generic_uses, fukey)) {
                map_put(fd->generic_uses, fukey, fu);
            }
            fc->def = fd;
            if (fd->is_variadic) {
                if (fd->args.length > fc->arguments.length) spanned_error("Too few args for variadic function", expr->span, "expected at least %llu args, got %llu", fd->args.length, fc->arguments.length);
            } else {
                if (fd->args.length != fc->arguments.length) spanned_error("Argument count mismatch", expr->span, "expected %llu args, got %llu", fd->args.length, fc->arguments.length);
            }
            list_foreach_i(&fc->arguments, lambda(void, (usize i, Expression* arg) {
                resolve_expr(program, module, func, arg, vars);
                if (i < fd->args.length) assert_types_equal(program, module, arg->resolved->type, fd->args.elements[i]->type, arg->span);
            }));
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = fd->return_type;
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING: {
                    expr->resolved = gc_malloc(sizeof(TVBox));
                    expr->resolved->type = gen_typevalue("::std::c_const_str_ptr", &expr->span);
                    resolve_typevalue(program, module, expr->resolved->type);
                } break;
                case NUMERAL: {
                    str num = lit->string;
                    str ty = "i32";
                    for (usize i = 0;i < strlen(num);i++) {
                        if (num[i] == 'u' || num[i] == 'i') ty = num + i;
                    }
                    expr->resolved = gc_malloc(sizeof(TVBox));
                    expr->resolved->type = gen_typevalue(to_str_writer(stream, fprintf(stream, "::std::%s", ty)), &expr->span);
                    resolve_typevalue(program, module, expr->resolved->type);
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
                resolve_typevalue(program, module, let->type);
                assert_types_equal(program, module, let->type, let->value->resolved->type, expr->span);
            }

            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = gen_typevalue("::std::unit", &expr->span);
            resolve_typevalue(program, module, expr->resolved->type);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            resolve_expr(program, module, func, cond->cond, vars);
            TypeValue* bool_ty = gen_typevalue("::std::bool", &cond->cond->span);
            resolve_typevalue(program, module, bool_ty);
            assert_types_equal(program, module, cond->cond->resolved->type, bool_ty, cond->cond->span);
            resolve_block(program, module, func, cond->then, vars);
            resolve_block(program, module, func, cond->otherwise, vars);
            assert_types_equal(program, module, cond->then->res, cond->otherwise->res, expr->span);
            expr->resolved = gc_malloc(sizeof(TVBox));
            expr->resolved->type = cond->then->res;
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
        resolve_typevalue(program, module, yield_ty->type);
    }
    block->res = yield_ty->type;

    vars->length = restore_len;
}

void resolve_funcdef(Program* program, Module* module, FuncDef* func) {
    if (func->return_type == NULL) {
        func->return_type = gen_typevalue("::std::unit", &func->name->span);
    }
    resolve_typevalue(program, module, func->return_type);
    if (func->body != NULL) {
        VarList vars = list_new(VarList);
        list_foreach(&func->args, lambda(void, (Argument* arg) {
            VarBox* v = var_register(&vars, arg->var);
            v->resolved->type = arg->type;
            resolve_typevalue(program, module, v->resolved->type);
        }));
        resolve_block(program, module, func, func->body, &vars);
        assert_types_equal(program, module, func->body->res, func->return_type, func->name->span);
    }
}

void resolve_module(Program* program, Module* module, Map* in_resolution) {
    if (module->resolved) return;
    str path_str = to_str_writer(stream, fprint_path(stream, module->path));
    if (map_contains(in_resolution, path_str)) panic("recursion detected while resolving %s", path_str);
    map_put(in_resolution, path_str, module);

    list_foreach(&module->imports, lambda(void, (Path* path){
        Module* mod = map_get(program->modules, to_str_writer(stream, fprint_path(stream, path)));
        resolve_module(program, mod, in_resolution);
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
    map_remove(in_resolution, path_str);
}

void resolve(Program* program) {
    Map* in_resolution = map_new();
    map_foreach(program->modules, lambda(void, (str key, Module* mod) {
        resolve_module(program, mod, in_resolution);
    }));
}
