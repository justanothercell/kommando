#pragma once

#include "tokens.c"
#include "types.c"
#include "lib/list.c"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum ExprType {
    FUNC_CALL_EXPR,
    BLOCK_EXPR,
    IF_EXPR,
    WHILE_EXPR,
    LITERAL_EXPR,
    VARIABLE_EXPR,
    RETURN_EXPR,
    STRUCT_LITERAL,
    REF_EXPR,
    DEREF_EXPR,
    VAR_DECL_EXPR,
    ASSIGN_EXPR,
} ExprType;

typedef struct Expression {
    ExprType type;
    void* expr;
} Expression;

typedef struct Block {
    Expression** exprs;
    usize expr_c;
} Block;

typedef struct Assign {
    Expression* target;
    Expression* value;
} Assign;

typedef struct IfExpr {
    Expression* condition;
    Block* then;
    Block* otherwise;
} IfExpr;

typedef struct VarDecl {
    Expression* value;
    char* type;
    char* name;
} VarDecl;

void drop_expression(Expression* expression);

void drop_var_decl(VarDecl* vd) {
    drop_expression(vd->value);
    free(vd->name);
    free(vd->type);
    free(vd);
}

void drop_assign(Assign* ass) {
    drop_expression(ass->target);
    drop_expression(ass->value);
    free(ass);
}

void drop_block(Block* block) {
    for (usize i = 0;i < block->expr_c;i++) {
        drop_expression(block->exprs[i]);
    }
    free(block->exprs);
    free(block);
}

void drop_if_expr(IfExpr* if_expr) {
    drop_expression(if_expr->condition);
    drop_block(if_expr->then);
    drop_block(if_expr->otherwise);
    free(if_expr);
}

typedef struct WhileExpr {
    Expression* condition;
    Block* body;
} WhileExpr;

void drop_while_expr(WhileExpr* while_expr) {
    drop_expression(while_expr->condition);
    drop_block(while_expr->body);
    free(while_expr);
}

typedef struct FunctionCall {
    char* name;
    Expression** args;
    usize args_c;
} FunctionCall;

void drop_function_call(FunctionCall* call) {
    for (usize i = 0;i < call->args_c;i++) {
        drop_expression(call->args[i]);
    }
    free(call->args);
    free(call);
}

void drop_expression(Expression* expr) {
    switch (expr->type) {
        case FUNC_CALL_EXPR:
            drop_function_call((FunctionCall*) expr->expr);
            break;
        case BLOCK_EXPR:
            drop_block((Block*) expr->expr);
            break;
        case IF_EXPR:
            drop_if_expr((IfExpr*) expr->expr);
            break;
        case WHILE_EXPR:
            drop_while_expr((WhileExpr*) expr->expr);
            break;
        case LITERAL_EXPR:
            drop_token((Token*) expr->expr);
            break;
        case VARIABLE_EXPR:
            free((char*) expr->expr);
            break;
        case REF_EXPR:
        case DEREF_EXPR:
        case RETURN_EXPR:
            drop_expression((Expression*) expr->expr);
            break;
        case STRUCT_LITERAL:
            free((char*) expr->expr);
            break;
        case VAR_DECL_EXPR:
            drop_var_decl((VarDecl*) expr->expr);
            break;
        case ASSIGN_EXPR:
            drop_assign((Assign*) expr->expr);
            break;
    }
    free(expr);
}

typedef struct FunctionDef {
    char* name;
    Block* body;
    char* ret_t;
    char** args_t;
    char** args;
    usize args_c;
} FunctionDef;

void drop_function_def(FunctionDef* func) {
    drop_block(func->body);
    free(func->name);
    free(func->ret_t);
    for (usize i = 0;i < func->args_c;i++) {
        free(func->args[i]);
        free(func->args_t[i]);    
    }
    free(func->args_t);
    free(func->args);
    free(func);
}

Block* parse_block(TokenStream* stream);

Expression* parse_expression(TokenStream* stream) {
    Expression* expr = malloc(sizeof(Expression));
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, "&") == 0) {
        drop_token(t);
        expr->type = REF_EXPR;
        expr->expr = parse_expression(stream);
    } else if (t->type == SNOWFLAKE && strcmp(t->string, "*") == 0) {
        drop_token(t);
        expr->type = DEREF_EXPR;
        expr->expr = parse_expression(stream);
    } else if (t->type == IDENTIFIER && strcmp(t->string, "return") == 0) {
        drop_token(t);
        expr->type = RETURN_EXPR;
        expr->expr = parse_expression(stream);
    } else if (t->type == IDENTIFIER && strcmp(t->string, "let") == 0) {
        drop_token(t);
        t = next_token(stream);
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        char* name = t->string;
        free(t);
        t = next_token(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        t = next_token(stream);
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        char* type = t->string;
        free(t);
        VarDecl* vd = malloc(sizeof(VarDecl));
        vd->name = name;
        vd->type = type;
        t = next_token(stream);
        if (t->type == SNOWFLAKE && strcmp(t->string, "=") == 0) {
            drop_token(t);
            vd->value = parse_expression(stream);
        } else {
            stream->peek = t;
            vd->value = NULL;
        }
        expr->type = VAR_DECL_EXPR;
        expr->expr = vd;
    } else if (t->type == STRING || t->type == NUMERAL) {
        expr->type = LITERAL_EXPR;
        expr->expr = t;
    } else if (t->type == IDENTIFIER) {
        char* name = t->string;
        free(t);
        if (strcmp(name, "if") == 0) {
            Expression* cond = parse_expression(stream);
            Block* body = parse_block(stream);
            IfExpr* if_expr = malloc(sizeof(IfExpr));
            if_expr->condition = cond;
            if_expr->then = body;
            t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
            if (t->type == IDENTIFIER && strcmp(t->string, "else") == 0) {
                drop_token(t);
                if_expr->otherwise = parse_block(stream);
            } else {
                if_expr->otherwise = NULL;
                stream->peek = t;
            }
            expr->type = IF_EXPR;
            expr->expr = if_expr;
        } else if (strcmp(name, "while") == 0) {
            Expression* cond = parse_expression(stream);
            Block* body = parse_block(stream);
            WhileExpr* while_expr = malloc(sizeof(WhileExpr));
            while_expr->condition = cond;
            while_expr->body = body;
            expr->type = WHILE_EXPR;
            expr->expr = while_expr;
        } else {
            Token* t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
            if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) {
                expr->type = VARIABLE_EXPR;
                expr->expr = name;
                stream->peek = t;
            } else {
                usize args_len = 0;
                usize capacity = 0;
                Expression** args = NULL;
                t = next_token(stream);
                if (t == NULL) unexpected_eof(stream);
                if (t->type == SNOWFLAKE && strcmp(t->string, ")") == 0) {
                    drop_token(t);
                } else {
                    stream->peek = t;
                    while (1) {
                        Expression* e = parse_expression(stream);
                        list_append(e, args, args_len, capacity);
                        t = next_token(stream);
                        if (t == NULL) unexpected_eof(stream);
                        if (t->type == SNOWFLAKE && strcmp(t->string, ")") == 0) {
                            drop_token(t);
                            break;
                        } else if (t->type != SNOWFLAKE || strcmp(t->string, ",") != 0) {
                            unexpected_token_error(t, stream);
                        }
                        drop_token(t); 
                    }
                }
                FunctionCall* call = malloc(sizeof(FunctionCall));
                call->name = name;
                call->args = args;
                call->args_c = args_len;
                expr->type = FUNC_CALL_EXPR;
                expr->expr = call;
            }
        }
    } else {
        unexpected_token_error(t, stream);
    }
    t = next_token(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, "=") == 0) {
        drop_token(t);
        Assign* assign = malloc(sizeof(Assign));
        assign->target = expr;
        assign->value = parse_expression(stream);
        expr = malloc(sizeof(Expression));
        expr->type = ASSIGN_EXPR;
        expr->expr = assign;
    } else {
        stream->peek = t;
    }
    return expr;
}

Block* parse_block(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, "{") != 0) {
        unexpected_token_error(t, stream);
    }
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    usize expr_len = 0;
    usize capacity = 0;
    Expression** exprs = NULL;
    
    while (t->type != SNOWFLAKE || strcmp(t->string, "}") != 0) {
        stream->peek = t;
        Expression* expr = parse_expression(stream);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ";") != 0) {
            unexpected_token_error(t, stream);
        }
        list_append(expr, exprs, expr_len, capacity);
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
    }
    drop_token(t);
    Block* block = malloc(sizeof(Block));
    block->exprs = exprs;
    block->expr_c = expr_len;
    return block;
}

char* parse_type_value(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    char* ty = malloc(strlen(t->string) + 1);
    strcpy(ty, t->string);
    ty[strlen(ty)] = '\0';
    while (t->type == SNOWFLAKE && strcmp(t->string, "&") == 0) {
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        usize old = strlen(ty);
        ty = realloc(ty, old + strlen(t->string) + 1);
        strcpy(old + ty, t->string);
        ty[strlen(ty)] = '\0';
    }
    drop_token(t);
    return ty;
}

TypeDef* parse_type_def(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER || strcmp(t->string, "type") != 0) unexpected_token_error(t, stream);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    char* name = t->string;
    free(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, "=") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER || strcmp(t->string, "struct") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, "{") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    usize fields_len = 0;
    usize fields_cap = 0;
    char** fields = NULL;
    usize fields_t_len = 0;
    usize fields_t_cap = 0;
    char** fields_t = NULL;
    while (t->type != SNOWFLAKE || strcmp(t->string, "}") != 0) {
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        char* field = t->string;
        free(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        char* field_t = t->string;
        free(t);
        list_append(field, fields, fields_len, fields_cap);
        list_append(field_t, fields_t, fields_t_len, fields_t_cap);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type == SNOWFLAKE && strcmp(t->string, ",") == 0) {
            drop_token(t);
            t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
        } else if (t->type != SNOWFLAKE || strcmp(t->string, "}") != 0) unexpected_token_error(t, stream);
    }
    drop_token(t);
    Struct* s = malloc(sizeof(Struct));
    usize l = strlen(name);
    char* sname = malloc(l + 1);
    strcpy(sname, name);
    sname[l] = '\0';
    s->name = sname;
    s->fields = fields;
    s->fields_c = fields_len;
    s->fields_t = fields_t;
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_STRUCT;
    type->ty = s;
    TypeDef* tydef = malloc(sizeof(TypeDef));
    tydef->name = name;
    tydef->type = type;
    return tydef;
}

FunctionDef* parse_function_def(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER || strcmp(t->string, "fn") != 0) unexpected_token_error(t, stream);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    char* name = t->string;
    free(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    usize args_len = 0;
    usize args_capacity = 0;
    usize args_t_len = 0;
    usize args_t_capacity = 0;
    char** args = NULL;
    char** args_t = NULL;
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    while (t->type != SNOWFLAKE || strcmp(t->string, ")") != 0) {
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        list_append(t->string, args, args_len, args_capacity);
        free(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        char* ty = parse_type_value(stream);
        list_append(ty, args_t, args_t_len, args_t_capacity);
        t = next_token(stream);
    }
    drop_token(t);

    Block* body = parse_block(stream);

    Expression* ret_value = malloc(sizeof(Expression));
    ret_value->type = STRUCT_LITERAL;
    ret_value->expr = "unit";
    Expression* ret = malloc(sizeof(Expression));
    ret->type = RETURN_EXPR;
    ret->expr = ret_value;

    // the capacity is buullshit but shhh
    usize fake_cap = body->expr_c;
    list_append(ret, body->exprs, body->expr_c, fake_cap);
    
    FunctionDef* func = malloc(sizeof(FunctionDef));
    if (strcmp(name, "main") == 0) func->name = "__entry__";
    else func->name = name;
    func->body = body;
    func->ret_t = "unit";
    func->args_c = args_len;
    func->args_t = args_t;
    func->args = args;
    return func;
}

typedef struct Module {
    FunctionDef** funcs;
    usize funcs_c;
    TypeDef** types;
    usize types_c;
} Module;

void drop_module(Module* module) {
    for (usize i = 0;i < module->funcs_c;i++) {
        drop_function_def(module->funcs[i]);
    }
    for (usize i = 0;i < module->types_c;i++) {
        drop_type_def(module->types[i]);
    }
    free(module->funcs);
    free(module->types);
    free(module);
}

Type* find_type(Module* module, char* type) {
    if (type[0] == '&') {
        Type* ty = malloc(sizeof(Type));
        ty->type = TYPE_POINTER;
        ty->ty = find_type(module, type+1);
        return ty;
    }
    for (usize i = 0;i < module->types_c;i++) {
        if (strcmp(type, module->types[i]->name) == 0) {
            return module->types[i]->type;
        }
    }
    printf("No such type `%s`", type);
    exit(1);
}

Module* parse_module(TokenStream* stream) {
    FunctionDef** funcs = NULL;
    usize funcs_len = 0;
    usize funcs_capacity = 0;
    TypeDef** types = NULL;
    usize types_len = 0;
    usize types_capacity = 0;
    while (1) {
        Token* t = next_token(stream);
        if (t == NULL) break;
        if (t->type != IDENTIFIER) {
            unexpected_token_error(t, stream);
        }
        if (strcmp(t->string, "fn") == 0) {
            stream->peek = t;
            FunctionDef* func = parse_function_def(stream);
            printf("parsed function `%s` with %lld args and %lld expressions\n", func->name, func->args_c, func->body->expr_c);
            list_append(func, funcs, funcs_len, funcs_capacity);
        } else if (strcmp(t->string, "type") == 0) {
            stream->peek = t;
            TypeDef* tydef = parse_type_def(stream);
            printf("parsed type `%s`\n", tydef->name);
            list_append(tydef, types, types_len, types_capacity);
        } else {
            unexpected_token_error(t, stream);
        }
        drop_token(t);
    }
    printf("parsed %lld functions and %lld types\n", funcs_len, types_len);
    usize tlen = types_len;
    register_builtin_types(&types, &types_len, &types_capacity);
    printf("registered %lld builtins\n", types_capacity - tlen);
    Module* module = malloc(sizeof(Module));
    module->funcs = funcs;
    module->funcs_c = funcs_len;
    module->types = types;
    module->types_c = types_len;
    return module;
}