#ifndef INFER_H
#define INFER_H

#include "lib/defines.h"

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Variable {
    str name;
    TypeValue* type;
} Variable;

void drop_variable(Variable var);

LIST(LocalScope, Variable);
LIST(ScopeStack, LocalScope);

void push_frame(ScopeStack* stack);

void drop_loal_scope(LocalScope frame);

void pop_frame(ScopeStack* stack);

TypeValue* get_var_type(ScopeStack* stack, str name);

void assert_type_match(TypeValue* type1, TypeValue* type2);
bool type_match(TypeValue* type1, TypeValue* type2);

void register_var(ScopeStack* stack, str name, TypeValue* type);

void infer_field_access(Module* module, ScopeStack* stack, TypeValue* parent_type, Expression* field);

TypeValue* degenerify(TypeValue* value, StrList* generics, TypeValueList* mapping, usize src_line);
TypeValue* degenerify_mapping(TypeValue* value, GenericsMapping* mapping, usize src_line);

#define infer_types_expression(module, stack, expr, mapping) TRACEV(__infer_types_expression(module, stack, expr, mapping))
void __infer_types_expression(Module* module, ScopeStack* stack, Expression* expr, GenericsMapping* mapping);

void infer_types_block(Module* module, ScopeStack* stack, Block* block, GenericsMapping* mapping);

void infer_types_fn(Module* module, ScopeStack* stack, FunctionDef* fn, GenericsMapping* mapping);

void infer_types(Module* module);

#endif