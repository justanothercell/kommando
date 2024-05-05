#ifndef INFER_H
#define INFER_H

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Variable {
    str name;
    str type;
} Variable;

void drop_variable(Variable var);

LIST(LocalScope, Variable);
LIST(ScopeStack, LocalScope);

void push_frame(ScopeStack* stack);

void drop_loal_scope(LocalScope frame);

void pop_frame(ScopeStack* stack);

str get_var_type(ScopeStack* stack, str name);

void assert_type_match(str type1, str type2, usize loc);

void register_var(ScopeStack* stack, str name, str type);

void infer_field_access(Module* module, ScopeStack* stack, str parent_type, Expression* field);

#define infer_types_expression(module, stack, expr) TRACEV(__infer_types_expression(module, stack, expr))
void __infer_types_expression(Module* module, ScopeStack* stack, Expression* expr);

void infer_types_block(Module* module, ScopeStack* stack, Block* block);

void infer_types_fn(Module* module, ScopeStack* stack, FunctionDef* fn);

void infer_types(Module* module);

#endif