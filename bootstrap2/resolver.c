#include "ast.h"
#include "lib.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/gc.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
#include "module.h"
#include "parser.h"

void resolve_typevalue(Program* program, Module* module, TypeValue* tval);
void assert_types_equal(TypeValue* tv1, TypeValue* tv2, Span span) {
    if (tv1->def == NULL) spanned_error("Unresolved type (1)", span, "%s", to_str_writer(stream, fprint_typevalue(stream, tv1)));
    if (tv2->def == NULL) spanned_error("Unresolved type (2)", span, "%s", to_str_writer(stream, fprint_typevalue(stream, tv2)));
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
    return NULL;
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
            
        } break;
        case EXPR_FUNC_CALL: {
            
        } break;
        case EXPR_LITERAL: {
            
        } break;
        case EXPR_BLOCK: {
            Block* block = expr->expr;
            resolve_block(program, module, func, block, vars);
        } break;
        case EXPR_VARIABLE: {
            Variable* var = expr->expr;
            var_find(vars, var);
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;
            VarBox* v = var_register(vars, let->var);
            expr->resolved = v->resolved;
            if (let->value != NULL) {
                resolve_expr(program, module, func, let->value, vars);
            }
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            resolve_expr(program, module, func, cond->cond, vars);
            resolve_block(program, module, func, cond->then, vars);
            resolve_block(program, module, func, cond->otherwise, vars);
        } break;
        case EXPR_WHILE_LOOP: {
            WhileLoop* wl = expr->expr;
            resolve_expr(program, module, func, wl->cond, vars);
            resolve_block(program, module, func, wl->body, vars);
        } break;
        case EXPR_RETURN: {
            resolve_expr(program, module, func, expr->expr, vars);
        } break;
        case EXPR_BREAK: {
            resolve_expr(program, module, func, expr->expr, vars);
        } break;
        case EXPR_CONTINUE: {
            todo("continue -> unit type");
        } break;
    }
}

void resolve_block(Program* program, Module* module, FuncDef* func, Block* block, VarList* vars) {
    usize restore_len = vars->length;
    
    list_foreach(&block->expressions, lambda(void, (Expression* expr) {
        resolve_expr(program, module, func, expr, vars);
    }));

    vars->length = restore_len;
}

void resolve_funcdef(Program* program, Module* module, FuncDef* func) {
    VarList vars = list_new(VarList);
    list_foreach(&func->args, lambda(void, (Argument* arg) {
        VarBox* v = var_register(&vars, arg->var);
    }));
    resolve_block(program, module, func, func->body, &vars);
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
    
    map_foreach(module->functions, lambda(void, (str key, FuncDef* func) {
        resolve_funcdef(program, module, func);
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
