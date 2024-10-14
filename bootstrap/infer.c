#include "infer.h"

#include "lib/defines.h"

#include "ast.h"

#include "lib/str.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void drop_variable(Variable var) {
    free(var.name);
    free(var.type);
}

void push_frame(ScopeStack* stack) {
    LocalScope frame = list_new();
    list_append(stack, frame);
}

void drop_loal_scope(LocalScope frame) {
    list_foreach(&frame, drop_variable);
    free(frame.elements);
}

void pop_frame(ScopeStack* stack) {
    drop_loal_scope(list_pop(stack));
}

TypeValue* get_var_type(ScopeStack* stack, str name) {
    if (strcmp(name, "NULL") == 0) {
        TypeValue* null = new_type_value(copy_str("any"), -1);
        null->indirection = 1;
        return null;
    }
    for (int i = stack->length - 1;i >= 0;i--) {
        for (int j = stack->elements[i].length - 1;j >= 0;j--) {
            if (strcmp(name, stack->elements[i].elements[j].name) == 0) {
                return stack->elements[i].elements[j].type;
            }
        }
    }
    fprintf(stderr, "No such variable %s\n", name);
    exit(1);
}

void types_mismatch_message(TypeValue* type1, TypeValue* type2, str message) {
    printf("Types `");
    fprint_type_value(stdout, type1);
    printf("`and `");
    fprint_type_value(stdout, type2);
    printf("` do not match (%s, near line %lld and %lld)\n", message, type1->src_line + 1, type2->src_line + 1);
    exit(1);
}

void assert_type_match(TypeValue* type1, TypeValue* type2) {
    if (strcmp(type1->name, "any") == 0) return;
    if (strcmp(type2->name, "any") == 0) return;
    if (strcmp(type1->name, type2->name) != 0) types_mismatch_message(type1, type2, "mismatched names");
    if (type1->indirection != type2->indirection) types_mismatch_message(type1, type2, "inequal levels of indirection");
    if (type1->generics.length != type2->generics.length) types_mismatch_message(type1, type2, "generics count mismatch");
    
    for (usize i = 0;i < type1->generics.length;i++) {
        assert_type_match(type1->generics.elements[i], type2->generics.elements[i]);
    }
}

bool type_match(TypeValue* type1, TypeValue* type2) {
    if (strcmp(type1->name, "any") == 0) return true;
    if (strcmp(type2->name, "any") == 0) return true;
    if (strcmp(type1->name, type2->name) != 0) return false;
    if (type1->indirection != type2->indirection) return false;
    if (type1->generics.length != type2->generics.length) return false;
    
    for (usize i = 0;i < type1->generics.length;i++) {
        if(!type_match(type1->generics.elements[i], type2->generics.elements[i])) return false;
    }
    return true;
}

void register_var(ScopeStack* stack, str name, TypeValue* type) {
    Variable var = { copy_str(name), copy_type_value(type, type->src_line) };
    list_append(&(stack->elements[stack->length-1]), var);
}

void infer_field_access(Module* module, ScopeStack* stack, TypeValue* parent_type, Expression* field) {
    switch (field->kind) {
        case FIELD_ACCESS_EXPR: {
            FieldAccess* fa = field->expr;
            infer_field_access(module, stack, parent_type, fa->object);
            infer_field_access(module, stack, fa->object->type, fa->field);
            field->type = copy_type_value(fa->field->type, field->src_line);
        } break;
        case VARIABLE_EXPR: {
            str field_name = field->expr;
            Type* t = find_type_def(module, parent_type)->type;
            switch (t->type) {
                case TYPE_PRIMITIVE:
                    fprintf(stderr, "primitive `");
                    fprint_type_value(stderr, parent_type);
                    fprintf(stderr, "` has no such field `%s` (near line %lld)\n", field_name, field->src_line + 1);
                    exit(1);
                case TYPE_STRUCT: {
                    Struct* s = t->ty;
                    for (usize i = 0;i < s->fields.length;i++) {
                        if (strcmp(s->fields.elements[i], field_name) == 0) {
                            field->type = copy_type_value(s->fields_t.elements[i], field->src_line);
                            goto found;
                        }
                    }
                    fprintf(stderr, "struct `%s` (`", s->name);
                    fprint_type_value(stderr, parent_type);
                    fprintf(stderr, "`) has no such field `%s` (near line %lld)\n", field_name, field->src_line + 1);
                    exit(1);
                    found: {}
                } break;
            }
        } break;
        default:
            fprintf(stderr, "unreachable field kind (%d)\n", field->kind);
            exit(1);
    }
}

TypeValue* degenerify_mapping(TypeValue* value, GenericsMapping* mapping, usize src_line) {
    if (mapping == NULL) return value;
    for (usize i = 0;i < mapping->length;i++) {
        if (strcmp(value->name, mapping->elements[i]->name) == 0) {
            TypeValue* t = mapping->elements[i]->type;
            TypeValue* v = malloc(sizeof(TypeValue));
            v->src_line = src_line;
            v->generics = (TypeValueList)list_new();
            v->indirection = t->indirection + value->indirection;
            v->name = copy_str(t->name);
            for (usize j = 0;j < t->generics.length;j++) {
                list_append(&v->generics, degenerify_mapping(t->generics.elements[j], mapping, src_line));
            }
            return v;
        }
    }
    TypeValue* v = malloc(sizeof(TypeValue));
    v->src_line = src_line;
    v->generics = (TypeValueList)list_new();
    v->indirection = value->indirection;
    v->name = copy_str(value->name);
    for (usize j = 0;j < value->generics.length;j++) {
        list_append(&v->generics, degenerify_mapping(value->generics.elements[j], mapping, src_line));
    }
    return v;
}

TypeValue* degenerify(TypeValue* value, StrList* generics, TypeValueList* mapping, usize src_line) {
    if (mapping == NULL) return value;
    for (usize i = 0;i < generics->length;i++) {
        if (strcmp(value->name, generics->elements[i]) == 0) {
            TypeValue* t = mapping->elements[i];
            TypeValue* v = malloc(sizeof(TypeValue));
            v->src_line = src_line;
            v->generics = (TypeValueList)list_new();
            v->indirection = t->indirection + value->indirection;
            v->name = copy_str(t->name);
            for (usize j = 0;j < t->generics.length;j++) {
                list_append(&v->generics, degenerify(t->generics.elements[j], generics, mapping, src_line));
            }
            return v;
        }
    }
    TypeValue* v = malloc(sizeof(TypeValue));
    v->src_line = src_line;
    v->generics = (TypeValueList)list_new();
    v->indirection = value->indirection;
    v->name = copy_str(value->name);
    for (usize j = 0;j < value->generics.length;j++) {
        list_append(&v->generics, degenerify(value->generics.elements[j], generics, mapping, src_line));
    }
    return v;
}

void __infer_types_expression(Module* module, ScopeStack* stack, Expression* expr, GenericsMapping* mapping) {
    switch (expr->kind) {
        case FUNC_CALL_EXPR: {
            FunctionCall* fc = expr->expr;
            FunctionDef* func = find_func_def(module, fc, expr->src_line);
            if (func->is_variadic) {
                if (func->args.length > fc->args.length) {
                    fprintf(stderr, "called variadic function with %lld args but expected at least %lld (near line %lld)", fc->args.length, func->args.length, expr->src_line);
                    exit(1);
                }
            } else {
                if (func->args.length != fc->args.length) {
                    fprintf(stderr, "called function with %lld args but expected %lld (near line %lld)", fc->args.length, func->args.length, expr->src_line);
                    exit(1);
                }
            }
            for (int i = 0;i < fc->args.length;i++) {
                infer_types_expression(module, stack, fc->args.elements[i], mapping);
                if (i < func->args.length) {
                    TypeValue* target = degenerify(func->args_t.elements[i], &func->generics, &fc->generics, expr->src_line);
                    assert_type_match(fc->args.elements[i]->type, target);
                    drop_type_value(target);
                }
            }
            expr->type = degenerify(func->ret_t, &func->generics, &fc->generics, expr->src_line);
        } break;
        case BLOCK_EXPR: {
            infer_types_block(module, stack, expr->expr, mapping);
            expr->type = new_type_value(copy_str("unit"), expr->src_line);
        } break;
        case IF_EXPR: {
            IfExpr* if_expr = expr->expr;
            infer_types_expression(module, stack, if_expr->condition, mapping);
            TypeValue* b = new_type_value(copy_str("bool"), if_expr->condition->src_line);
            assert_type_match(if_expr->condition->type, b);
            drop_type_value(b);
            infer_types_block(module, stack, if_expr->then, mapping);
            if (if_expr->otherwise != NULL) infer_types_block(module, stack, if_expr->otherwise, mapping);
            expr->type = new_type_value(copy_str("unit"), expr->src_line);
        } break;
        case WHILE_EXPR: {
            WhileExpr* while_expr = expr->expr;
            infer_types_expression(module, stack, while_expr->condition, mapping);
            TypeValue* b = new_type_value(copy_str("bool"), while_expr->condition->src_line);
            assert_type_match(while_expr->condition->type, b);
            drop_type_value(b);
            infer_types_block(module, stack, while_expr->body, mapping);
            expr->type = new_type_value(copy_str("unit"), expr->src_line);
        } break;
        case LITERAL_EXPR: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING: {
                    TypeValue* str = new_type_value(copy_str("char"), expr->src_line);
                    str->indirection = 1;
                    expr->type = str;
                } break;
                case NUMERAL: {
                    usize len = strlen(lit->string);
                    for (int i = 0;i < len;i++) {
                        if (lit->string[i] == 'u' || lit->string[i] == 'i' || lit->string[i] == 'f' || lit->string[i] == 'b') {
                            expr->type = new_type_value(copy_str(lit->string+i), expr->src_line);
                            lit->string[i] = '\0';
                            goto end;
                        }
                    }
                    expr->type = new_type_value(copy_str("i32"), expr->src_line);
                    end: {}
                } break;
                default:
                    fprintf(stderr, "unreachable case: %s\n", TOKENTYPE_STRING[lit->type]);
                    exit(1);
            }
        } break;
        case VARIABLE_EXPR: {
            str var = expr->expr;
            expr->type = copy_type_value(get_var_type(stack, var), expr->src_line);
            find_type_def(module, expr->type); // register
        } break;
        case RETURN_EXPR: {
            infer_types_expression(module, stack, expr->expr, mapping);
            expr->type = new_type_value("unit", expr->src_line);
        } break;
        case STRUCT_LITERAL: {
            if (strcmp(expr->expr, "unit") == 0) {
                expr->type = new_type_value("unit", expr->src_line);
            } else {
                fprintf(stderr, "todo: STRUCT_LITERAL infer (%lld)\n", expr->src_line + 1);
                exit(1);   
            }
        } break;
        case REF_EXPR: {
            infer_types_expression(module, stack, expr->expr, mapping);
            TypeValue* inner = ((Expression*)expr->expr)->type;
            expr->type = copy_type_value(inner, expr->src_line);
            expr->type->indirection += 1;
        } break;
        case DEREF_EXPR: {
            infer_types_expression(module, stack, expr->expr, mapping);
            TypeValue* inner = ((Expression*)expr->expr)->type;
            if (inner->indirection == 0) {
                fprintf(stderr, "Cannot dereference type `");
                fprint_type_value(stderr, inner);
                fprintf(stderr, "` (near line %lld)\n", expr->src_line);
                exit(1);
            }
            expr->type = copy_type_value(inner, expr->src_line);
            expr->type->indirection -= 1;
        } break;
        case VAR_DECL_EXPR: {
            VarDecl* vd = expr->expr;
            if (vd->value != NULL) {
                infer_types_expression(module, stack, vd->value, mapping);
                assert_type_match(vd->type, vd->value->type);
            }
            register_var(stack, vd->name, vd->type);
            find_type_def(module, vd->type); // register
            expr->type = new_type_value("unit", expr->src_line);
        } break;
        case ASSIGN_EXPR: {
            Assign* ass = expr->expr;
            infer_types_expression(module, stack, ass->target, mapping);
            infer_types_expression(module, stack, ass->value, mapping);
            assert_type_match(ass->target->type, ass->value->type);
            expr->type = new_type_value("unit", expr->src_line);
        } break;
        case FIELD_ACCESS_EXPR: {
            FieldAccess* fa = expr->expr;
            infer_types_expression(module, stack, fa->object, mapping);
            infer_field_access(module, stack, fa->object->type, fa->field);
            expr->type = copy_type_value(fa->field->type, expr->src_line);
            find_type_def(module, expr->type); // register
            expr->type->src_line = expr->src_line;
        } break;
        case BINOP_EXPR: {
            BinOp* bo = expr->expr;
            infer_types_expression(module, stack, bo->lhs, mapping);
            infer_types_expression(module, stack, bo->rhs, mapping);
            assert_type_match(bo->lhs->type, bo->rhs->type);
            if (strcmp(bo->op, "&&") == 0 || strcmp(bo->op, "||") == 0) {
                TypeValue* b = new_type_value(copy_str("bool"), expr->src_line);
                assert_type_match(bo->lhs->type, b);
                expr->type = b;
            } else if (bo->op[0] == '!' || bo->op[0] == '=' || bo->op[0] == '>' || bo->op[0] == '<') {
                expr->type = new_type_value(copy_str("bool"), expr->src_line);
            } else {
                expr->type = copy_type_value(bo->lhs->type, expr->src_line);
            } 
        } break;
    }
}

void infer_types_block(Module* module, ScopeStack* stack, Block* block, GenericsMapping* mapping) {
    push_frame(stack);
    for (usize i = 0;i < block->exprs.length;i++) {
        Expression* expr = block->exprs.elements[i];
        infer_types_expression(module, stack, expr, mapping);
    }
    pop_frame(stack);
}

void infer_types_fn(Module* module, ScopeStack* stack, FunctionDef* fn, GenericsMapping* mapping) {
    push_frame(stack);
    for (int i = 0;i < fn->args.length;i++) {
        TypeValue* tv = degenerify_mapping(fn->args_t.elements[i], mapping, fn->args_t.elements[i]->src_line);
        find_type_def(module, tv); // register
        register_var(stack, fn->args.elements[i], tv);
    }
    infer_types_block(module, stack, fn->body, mapping);
    pop_frame(stack);
}

void infer_types(Module* module) {
    ScopeStack stack = list_new();
    for (usize i = 0;i < module->funcs.length;i++) {
        FunctionDef* func = module->funcs.elements[i];
        if (func->body == NULL) continue; 
        if (func->generics.length == 0) infer_types_fn(module, &stack, func, NULL);
        else for (usize j = 0;j < func->mappings.length;j++) {
            infer_types_fn(module, &stack, func, &func->mappings.elements[j]);
        }
        printf("    inferred types for function `");
        fprint_func(stdout, func);
        printf("`\n");
    }
    free(stack.elements);
}