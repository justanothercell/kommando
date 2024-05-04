#pragma once

#include "tokens.c"
#include "types.c"
#include "lib/list.c"
#include <limits.h>
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
    FIELD_ACCESS_EXPR,
    BINOP_EXPR
} ExprType;

typedef struct Expression {
    ExprType kind;
    any expr;
    str type;
    usize src_line;
} Expression;

LIST(ExpressionList, Expression*);

typedef struct Block {
    ExpressionList exprs;
} Block;

typedef struct Assign {
    Expression* target;
    Expression* value;
} Assign;

typedef struct BinOp {
    Expression* lhs;
    Expression* rhs;
    str op;
    usize precedence;
} BinOp;


typedef struct FieldAccess {
    Expression* object;
    Expression* field;
} FieldAccess;

typedef struct IfExpr {
    Expression* condition;
    Block* then;
    Block* otherwise;
} IfExpr;

typedef struct VarDecl {
    Expression* value;
    str type;
    str name;
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

void drop_binop(BinOp* op) {
    drop_expression(op->lhs);
    drop_expression(op->rhs);
    free(op->op);
    free(op);
}

void drop_field_access(FieldAccess* fa) {
    drop_expression(fa->object);
    drop_expression(fa->object);
    free(fa);
}

void drop_block(Block* block) {
    list_foreach(&(block->exprs), drop_expression);
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
    str name;
    ExpressionList args;
} FunctionCall;

void drop_function_call(FunctionCall* call) {
    list_foreach(&(call->args), drop_expression);
    free(call);
}

void drop_expression(Expression* expr) {
    switch (expr->kind) {
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
            free((str) expr->expr);
            break;
        case REF_EXPR:
        case DEREF_EXPR:
        case RETURN_EXPR:
            drop_expression((Expression*) expr->expr);
            break;
        case STRUCT_LITERAL:
            free((str) expr->expr);
            break;
        case VAR_DECL_EXPR:
            drop_var_decl((VarDecl*) expr->expr);
            break;
        case ASSIGN_EXPR:
            drop_assign((Assign*) expr->expr);
            break;
        case FIELD_ACCESS_EXPR:
            drop_field_access((FieldAccess*) expr->expr);
            break;
        case BINOP_EXPR:
            drop_binop((BinOp*) expr->expr);
            break;
    }
    free(expr->type);
    free(expr);
}

typedef struct FunctionDef {
    str name;
    Block* body;
    str ret_t;
    StringList args_t;
    StringList args;
} FunctionDef;

void drop_function_def(FunctionDef* func) {
    drop_block(func->body);
    free(func->name);
    free(func->ret_t);
    list_foreach(&(func->args), free);
    list_foreach(&(func->args_t), free);
    free(func);
}

#define parse_block(stream) TRACE(__parse_block(stream))
Block* __parse_block(TokenStream* stream);

#define parse_type_value(stream) TRACE(__parse_type_value(stream))
str __parse_type_value(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    str ty = malloc(strlen(t->string) + 1);
    strcpy(ty, t->string);
    ty[strlen(ty)] = '\0';
    while (t->type == SNOWFLAKE && strcmp(t->string, "&") == 0) {
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        usize old = strlen(ty);
        usize len = old + strlen(t->string);
        ty = realloc(ty, len + 1);
        strcpy(ty + old, t->string);
        ty[len] = '\0';
    }
    drop_token(t);
    return ty;
}

#define parse_expression(stream) TRACE(__parse_expression(stream))
Expression* __parse_expression(TokenStream* stream);

#define parse_subscript(stream) TRACE(__parse_subscript(stream))
Expression* __parse_subscript(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    str field = t->string;
    free(t);
    Expression* expr = malloc(sizeof(Expression));
    expr->kind = VARIABLE_EXPR;
    expr->expr = field;
    expr->src_line = stream->line;
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, ".") == 0) {
        drop_token(t);
        Expression* field = parse_subscript(stream);
        FieldAccess* fa = malloc(sizeof(FieldAccess));
        fa->object = expr;
        fa->field = field;
        expr = malloc(sizeof(Expression));
        expr->kind = FIELD_ACCESS_EXPR;
        expr->expr = fa;
    } else {
        stream->peek = t;
    }
    return expr;
}

#define parse_expresslet(stream) TRACE(__parse_expresslet(stream))
Expression* __parse_expresslet(TokenStream* stream) {
    Expression* expr = malloc(sizeof(Expression));
    expr->src_line = stream->line;
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, "{") == 0) {
        stream->peek = t;
        expr->kind = BLOCK_EXPR;
        expr->expr = parse_block(stream);
    } else if (t->type == SNOWFLAKE && strcmp(t->string, "(") == 0) {
        drop_token(t);
        expr = parse_expression(stream);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ")") != 0) unexpected_token_error(t, stream);
        drop_token(t);
    } else if (t->type == STRING || t->type == NUMERAL) {
        expr->kind = LITERAL_EXPR;
        expr->expr = t;
    } else if (t->type == IDENTIFIER) {
        str name = t->string;
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
            expr->kind = IF_EXPR;
            expr->expr = if_expr;
        } else if (strcmp(name, "while") == 0) {
            Expression* cond = parse_expression(stream);
            Block* body = parse_block(stream);
            WhileExpr* while_expr = malloc(sizeof(WhileExpr));
            while_expr->condition = cond;
            while_expr->body = body;
            expr->kind = WHILE_EXPR;
            expr->expr = while_expr;
        } else {
            Token* t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
            if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) {
                stream->peek = t;
                if (strcmp(name, "true") == 0) {
                    Token* tok = malloc(sizeof(Token));
                    tok->type = NUMERAL;
                    tok->string = "1bool";
                    expr->kind = LITERAL_EXPR;
                    expr->expr = tok;
                    free(name);
                } else if (strcmp(name, "false") == 0) {
                    Token* tok = malloc(sizeof(Token));
                    tok->type = NUMERAL;
                    tok->string = "0bool";
                    expr->kind = LITERAL_EXPR;
                    expr->expr = tok;
                    free(name);
                } else {
                    expr->kind = VARIABLE_EXPR;
                    expr->expr = name;
                }
            } else {
                ExpressionList args = list_new();
                t = next_token(stream);
                if (t == NULL) unexpected_eof(stream);
                if (t->type == SNOWFLAKE && strcmp(t->string, ")") == 0) {
                    drop_token(t);
                } else {
                    stream->peek = t;
                    while (1) {
                        Expression* e = parse_expression(stream);
                        list_append(&args, e);
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
                expr->kind = FUNC_CALL_EXPR;
                expr->expr = call;
            }
        }
    } else {
        unexpected_token_error(t, stream);
    }
    t = next_token(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, ".") == 0) {
        drop_token(t);
        Expression* field = parse_subscript(stream);
        FieldAccess* fa = malloc(sizeof(FieldAccess));
        fa->object = expr;
        fa->field = field; 
        expr = malloc(sizeof(Expression));
        expr->src_line = stream->line;
        expr->kind = FIELD_ACCESS_EXPR;
        expr->expr = fa;
    } else {
        stream->peek = t;
    }
    return expr; 
}

Expression* __parse_expression(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type == IDENTIFIER && strcmp(t->string, "return") == 0) {
        Expression* expr = malloc(sizeof(Expression));
        drop_token(t);
        expr->kind = RETURN_EXPR;
        expr->expr = parse_expression(stream);
        expr->src_line = stream->line;
        return expr;
    }
    if (t->type == IDENTIFIER && strcmp(t->string, "let") == 0) {
        Expression* expr = malloc(sizeof(Expression));
        expr->src_line = stream->line;
        drop_token(t);
        t = next_token(stream);
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        str name = t->string;
        free(t);
        t = next_token(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        str type = parse_type_value(stream);
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
        expr->kind = VAR_DECL_EXPR;
        expr->expr = vd;
        return expr;
    }
    Expression* expr = NULL;
    if (t->type == SNOWFLAKE && strcmp(t->string, "&") == 0) {
        drop_token(t);
        expr = malloc(sizeof(Expression));
        expr->kind = REF_EXPR;
        expr->expr = parse_expresslet(stream);
    } else if (t->type == SNOWFLAKE && strcmp(t->string, "*") == 0) {
        drop_token(t);
        expr = malloc(sizeof(Expression));
        expr->kind = DEREF_EXPR;
        expr->expr = parse_expresslet(stream);
    } else {
        stream->peek = t;
        expr = parse_expresslet(stream);
    }
    expr->src_line = stream->line;
    t = next_token(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, ")") == 0) {
        stream->peek = t;
        return expr;
    } 
    if (t->type == SNOWFLAKE && strcmp(t->string, ";") == 0) {
        stream->peek = t;
        return expr;
    } 
    if (t->type == SNOWFLAKE && strcmp(t->string, "{") == 0) {
        stream->peek = t;
        return expr;
    } 
    if (t->type == SNOWFLAKE && strcmp(t->string, ",") == 0) {
        stream->peek = t;
        return expr;
    } 
    if (t->type == SNOWFLAKE && strcmp(t->string, "=") == 0) {
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type == SNOWFLAKE && strcmp(t->string, "=") == 0) {
            goto binop; // '==' is now just '='
        }
        stream->peek = t;
        Assign* assign = malloc(sizeof(Assign));
        assign->target = expr;
        assign->value = parse_expression(stream);
        expr = malloc(sizeof(Expression));
        expr->src_line = stream->line;
        expr->kind = ASSIGN_EXPR;
        expr->expr = assign;
        return expr;
    } 
    binop: { }
    str op = NULL;
    usize op_len = 0;
    usize op_cap = 0;
    while (t->type == SNOWFLAKE && (t->string[0] == '=' || t->string[0] == '!' || t->string[0] == '&' || t->string[0] == '|' || t->string[0] == '>' || t->string[0] == '<' || t->string[0] == '+' || t->string[0] == '-' || t->string[0] == '*' || t->string[0] == '/' || t->string[0] == '%')) {
        list_append_raw(t->string[0], op, op_len, op_cap);
        free(t);
        t = next_token(stream);
    }
    stream->peek = t;
    list_append_raw('\0', op, op_len, op_cap);
    if (op_len == 1) {
        return expr; // no binop
    }
    if (strcmp(op, "=") == 0) { // artifact from the jump
        free(op);
        op = "==";
    }
    usize precedence;
    BinOp* binop = malloc(sizeof(BinOp));
    binop->lhs = expr;
    binop->rhs = parse_expression(stream);
    binop->op = op;
    binop->precedence = precedence;
    expr = malloc(sizeof(Expression));
    expr->src_line = stream->line;
    expr->kind = BINOP_EXPR;
    expr->expr = binop;
    return expr;
}

Block* __parse_block(TokenStream* stream) {
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
    ExpressionList exprs = list_new();
    
    while (t->type != SNOWFLAKE || strcmp(t->string, "}") != 0) {
        stream->peek = t;
        Expression* expr = parse_expression(stream);
        expr->src_line = stream->line;
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ";") != 0) {
            unexpected_token_error(t, stream);
        }
        list_append(&exprs, expr);
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
    }
    drop_token(t);
    Block* block = malloc(sizeof(Block));
    block->exprs = exprs;
    return block;
}

#define parse_type_def(stream) TRACE(__parse_type_def(stream))
TypeDef* __parse_type_def(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER || strcmp(t->string, "type") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    str name = t->string;
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
    StringList fields = list_new();
    StringList fields_t = list_new();
    while (t->type != SNOWFLAKE || strcmp(t->string, "}") != 0) {
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        str field = t->string;
        free(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        str field_t = parse_type_value(stream);
        list_append(&fields, field);
        list_append(&fields_t, field_t);
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
    str sname = malloc(l + 1);
    strcpy(sname, name);
    sname[l] = '\0';
    s->name = sname;
    s->fields = fields;
    s->fields_t = fields_t;
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_STRUCT;
    type->ty = s;
    TypeDef* tydef = malloc(sizeof(TypeDef));
    tydef->name = name;
    tydef->type = type;
    return tydef;
}

#define parse_function_def(stream) TRACE(__parse_function_def(stream))
FunctionDef* __parse_function_def(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER || strcmp(t->string, "fn") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    str name = t->string;
    free(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    StringList args = list_new();
    StringList args_t = list_new();
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    while (t->type != SNOWFLAKE || strcmp(t->string, ")") != 0) {
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        list_append(&args, t->string);
        free(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        str ty = parse_type_value(stream);
        list_append(&args_t, ty);
        t = next_token(stream);
        if (t->type == SNOWFLAKE && strcmp(t->string, ",") == 0) {
            drop_token(t);
            t = next_token(stream);
        } else if (t->type != SNOWFLAKE || strcmp(t->string, ")") != 0) unexpected_token_error(t, stream);
    }
    drop_token(t);

    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);

    FunctionDef* func = malloc(sizeof(FunctionDef));
    if (t->type == SNOWFLAKE && strcmp(t->string, "-") == 0) {
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ">") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        func->ret_t = parse_type_value(stream);
    } else {
        stream->peek = t;
        func->ret_t = NULL;
    }

    Block* body = NULL;

    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, ";") != 0) {
        stream->peek = t;
        
        body = parse_block(stream);

        if (func->ret_t == NULL) {
            Expression* ret_value = malloc(sizeof(Expression));
            ret_value->src_line = stream->line;
            ret_value->kind = STRUCT_LITERAL;
            ret_value->expr = "unit";
            Expression* ret = malloc(sizeof(Expression));
            ret->src_line = stream->line;
            ret->kind = RETURN_EXPR;
            ret->expr = ret_value;

            list_append(&(body->exprs), ret);
            func->ret_t = "unit";
        }
    } else {
        drop_token(t);
    }

    func->name = name;
    func->body = body;
    func->args_t = args_t;
    func->args = args;
    return func;
}

typedef struct Module {
    FunctionDefList funcs;
    TypeDefList types;
} Module;

void drop_module(Module* module) {
    list_foreach(&(module->funcs), drop_function_def);
    list_foreach(&(module->types), drop_type_def);
    free(module);
}

Type* find_type(Module* module, str type) {
    if (type[0] == '&') {
        Type* ty = malloc(sizeof(Type));
        ty->type = TYPE_POINTER;
        ty->ty = find_type(module, type+1);
        return ty;
    }
    for (usize i = 0;i < module->types.length;i++) {
        if (strcmp(type, module->types.elements[i]->name) == 0) {
            return module->types.elements[i]->type;
        }
    }
    printf("No such type `%s`\n", type);
    exit(1);
}

FunctionDef* find_func(Module* module, str func) {
    for (usize i = 0;i < module->funcs.length;i++) {
        if (strcmp(func, module->funcs.elements[i]->name) == 0) {
            return module->funcs.elements[i];
        }
    }
    printf("No such function `%s`\n", func);
    exit(1);
}

#define parse_module(stream)  TRACE(_parse_module(stream))
Module* _parse_module(TokenStream* stream) {
    FunctionDefList funcs = list_new();
    TypeDefList types = list_new();
    while (1) {
        Token* t = next_token(stream);
        if (t == NULL) break;
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        if (strcmp(t->string, "use") == 0) {
            drop_token(t);
            t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
            if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
            str fname = malloc(strlen(t->string) + 4);
            strcpy(fname, t->string);
            strcpy(fname+strlen(t->string), ".kdo");
            fname[strlen(t->string) + 4] = '\0';
            drop_token(t);
            t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
            if (t->type != SNOWFLAKE || strcmp(t->string, ";") != 0) unexpected_token_error(t, stream);
            drop_token(t);
            printf("importing %s\n", fname);
            FILE *source = fopen(fname, "r");
            TokenStream* stream = new_tokenstream(source);
            Module* module = parse_module(stream);
            drop_tokenstream(stream);
            list_extend(&funcs, &(module->funcs));
            list_extend(&types, &(module->types));
            printf("finished importing %s\n", fname);
        } else if (strcmp(t->string, "fn") == 0) {
            stream->peek = t;
            FunctionDef* func = parse_function_def(stream);
            if (func->body == NULL) printf("    parsed function definition `%s` with %lld args\n", func->name, func->args.length);
            else printf("    parsed function `%s` with %lld args and %lld expressions\n", func->name, func->args.length, func->body->exprs.length);
            list_append(&funcs, func);
        } else if (strcmp(t->string, "type") == 0) {
            stream->peek = t;
            TypeDef* tydef = parse_type_def(stream);
            printf("    parsed type `%s`\n", tydef->name);
            list_append(&types, tydef);
        } else {
            unexpected_token_error(t, stream);
        }
    }
    printf("parsed %lld functions and %lld types\n", funcs.length, types.length);
    Module* module = malloc(sizeof(Module));
    module->funcs = funcs;
    module->types = types;
    return module;
}