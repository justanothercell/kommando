#pragma once

#include "tokens.c"
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
    VARIABLE_EXPR
} ExprType;

typedef struct IfExpr {
    ExprType type;
} IfExpr;

typedef struct WhileExpr {
    ExprType type;
} WhileExpr;

typedef struct Expression {
    ExprType type;
    void* expr;
} Expression;

typedef struct Block {
    Expression** exprs;
    usize expr_c;
} Block;

typedef struct Type {
    char* name;
} Type;

void drop_type(Type* type) {
    free(type->name);
    free(type);
}

void drop_expression(Expression* expr);

void drop_block(Block* block) {
    for (usize i = 0;i < block->expr_c;i++) {
        drop_expression(block->exprs[i]);
    }
    free(block);
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
            break;
        case WHILE_EXPR:
            break;
        case LITERAL_EXPR:
            drop_token((Token*) expr->expr);
            break;
        case VARIABLE_EXPR:
            free((char*) expr->expr);
            break;
    }
    free(expr);
}

typedef struct FunctionDef {
    char* name;
    Block* body;
    Type* ret_t;
    Type** args_t;
    char** args;
    usize arg_c;
} FunctionDef;

void drop_function_def(FunctionDef* func) {
    drop_block(func->body);
    free(func->name);
    free(func);
}

Expression* parse_expression(TokenStream* stream) {
    Expression* expr = malloc(sizeof(Expression));
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type == STRING) {
        expr->type = LITERAL_EXPR;
        expr->expr = t;
    } else if (t->type == IDENTIFIER) {
        char* name = t->string;
        free(t);
        Token* t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) {
            expr->type = VARIABLE_EXPR;
            expr->expr = name;
            stream->peek = t;
        } else {
            expr->type = FUNC_CALL_EXPR;
            usize args_len = 0;
            usize capacity = 0;
            Expression** args = NULL;
            while (1) {
                t = next_token(stream);
                if (t == NULL) unexpected_eof(stream);
                if (t->type == SNOWFLAKE && strcmp(t->string, ")") == 0) {
                    drop_token(t);
                    break;
                }
                stream->peek = t;
                Expression* e = parse_expression(stream);
                list_append(e, args, args_len, capacity);
            }
            FunctionCall* call = malloc(sizeof(FunctionCall));
            call->name = name;
            call->args = args;
            call->args_c = args_len;
            expr->expr = call;
        }
    } else {
        unexpected_token_error(t, stream);
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

FunctionDef* parse_function_def(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) {
        unexpected_token_error(t, stream);
    }
    char* name = t->string;
    free(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) {
        unexpected_token_error(t, stream);
    }
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, ")") != 0) {
        unexpected_token_error(t, stream);
    }
    drop_token(t);

    Block* body = parse_block(stream);
    
    FunctionDef* func = malloc(sizeof(FunctionDef));
    if (strcmp(name, "main") == 0) func->name = "__entry__";
    else func->name = name;
    func->body = body;
    Type* ret = malloc(sizeof(Type));
    ret->name = "void";
    func->ret_t = ret;
    func->arg_c = 0;
    func->args_t = NULL;
    func->args = NULL;
    return func;
}

typedef struct Module {
    FunctionDef** funcs;
    usize funcs_c;
} Module;

void drop_module(Module* module) {
    for (usize i = 0;i < module->funcs_c;i++) {
        drop_function_def(module->funcs[i]);
    }
    free(module);
}

Module* parse_module(TokenStream* stream) {
    FunctionDef** funcs = NULL;
    usize funcs_len = 0;
    usize funcs_capacity = 0;
    while (1) {
        Token* t = next_token(stream);
        if (t == NULL) break;
        if (t->type != IDENTIFIER) {
            unexpected_token_error(t, stream);
        }
        if (strcmp(t->string, "fn") == 0) {
            FunctionDef* func = parse_function_def(stream);
            printf("parsed function `%s` with %lld expression(s)\n", func->name, func->body->expr_c);
            list_append(func, funcs, funcs_len, funcs_capacity);
        } else {
            unexpected_token_error(t, stream);
        }
        drop_token(t);
    }
    Module* module = malloc(sizeof(Module));
    module->funcs = funcs;
    module->funcs_c = funcs_len;
    return module;
}