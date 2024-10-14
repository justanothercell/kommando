#include "ast.h"

#include "lib/defines.h"

#include "lib/list.h"
#include "lib/str.h"

#include "tokens.h"
#include "types.h"
#include "infer.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void drop_var_decl(VarDecl* vd) {
    if (vd->value != NULL) drop_expression(vd->value);
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
    drop_expression(fa->field);
    free(fa);
}

void drop_block(Block* block) {
    list_foreach(&(block->exprs), drop_expression);
    free(block->exprs.elements);
    free(block);
}

void drop_if_expr(IfExpr* if_expr) {
    drop_expression(if_expr->condition);
    drop_block(if_expr->then);
    if (if_expr->otherwise != NULL) drop_block(if_expr->otherwise);
    free(if_expr);
}

void drop_while_expr(WhileExpr* while_expr) {
    drop_expression(while_expr->condition);
    drop_block(while_expr->body);
    free(while_expr);
}

void drop_function_call(FunctionCall* call) {
    list_foreach(&(call->args), drop_expression);
    free(call->args.elements);
    list_foreach(&(call->generics), drop_type_value);
    free(call->generics.elements);
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

void drop_function_def(FunctionDef* func) {
    if (func->body != NULL) drop_block(func->body);
    free(func->name);
    free(func->ret_t);
    list_foreach(&(func->args), free);
    list_foreach(&(func->args_t), free);
    list_foreach(&(func->generics), free);
    list_foreach(&(func->mappings), drop_generic_mapping);
    free(func->args.elements);
    free(func->args_t.elements);
    free(func->generics.elements);
    free(func->mappings.elements);
    free(func);
}

TypeValue* __parse_type_value(TokenStream* stream) {
    usize src_line = stream->line;
    usize indirection = 0;
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    while (t->type == SNOWFLAKE && strcmp(t->string, "&") == 0) {
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        indirection += 1;
    }
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    str name = t->string;
    free(t);
    TypeValueList generics = parse_generics_call(stream);
    TypeValue* ty = malloc(sizeof(TypeValue));
    ty->generics = generics;
    ty->indirection = indirection;
    ty->name = name;
    ty->src_line = src_line;
    return ty;
}

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
        expr->src_line = stream->line;
        expr->kind = FIELD_ACCESS_EXPR;
        expr->expr = fa;
    } else {
        stream->peek = t;
    }
    return expr;
}

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
        free(expr);
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
            if (t->type != SNOWFLAKE || (strcmp(t->string, "(") != 0 && strcmp(t->string, ":") != 0)) {
                stream->peek = t;
                if (strcmp(name, "true") == 0) {
                    Token* tok = malloc(sizeof(Token));
                    tok->type = NUMERAL;
                    tok->string = copy_str("1bool");
                    expr->kind = LITERAL_EXPR;
                    expr->expr = tok;
                    free(name);
                } else if (strcmp(name, "false") == 0) {
                    Token* tok = malloc(sizeof(Token));
                    tok->type = NUMERAL;
                    tok->string = copy_str("0bool");
                    expr->kind = LITERAL_EXPR;
                    expr->expr = tok;
                    free(name);
                } else {
                    expr->kind = VARIABLE_EXPR;
                    expr->expr = name;
                }
            } else {
                TypeValueList generics = list_new();
                if (t->type == SNOWFLAKE && strcmp(t->string, ":") == 0) {
                    drop_token(t);
                    t = next_token(stream);
                    if (t == NULL) unexpected_eof(stream);
                    if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
                    drop_token(t);
                    generics = parse_generics_call(stream);
                    t = next_token(stream);
                    if (t == NULL) unexpected_eof(stream);
                }
                if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) unexpected_token_error(t, stream);
                drop_token(t);
                ExpressionList args = list_new();
                t = next_token(stream);
                if (t == NULL) unexpected_eof(stream);
                if (t->type == SNOWFLAKE && strcmp(t->string, ")") == 0) {
                    drop_token(t);
                } else {
                    stream->peek = t;
                    while (1) {
                        Expression* e = parse_expression(stream);
                        printf("%p\n", e);
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
                call->generics = generics;
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
        TypeValue* type = parse_type_value(stream);
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
        op = copy_str("==");
    }
    usize precedence = 0;
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

StrList __parse_generics_def(TokenStream* stream) {
    StrList generics = list_new();
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, "<") == 0) {
        drop_token(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        while (t->type != SNOWFLAKE || strcmp(t->string, ">") != 0) {
            if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
            str generic = t->string;
            list_append(&generics, generic);
            free(t);
            t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
            if (t->type == SNOWFLAKE && strcmp(t->string, ",") == 0) {
                drop_token(t);
                t = next_token(stream);
                if (t == NULL) unexpected_eof(stream);
            } else if (t->type != SNOWFLAKE || strcmp(t->string, ">") != 0) unexpected_token_error(t, stream);
        }    
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
    }
    stream->peek = t;
    return generics;
}

TypeValueList __parse_generics_call(TokenStream* stream) {
    TypeValueList generics = list_new();
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type == SNOWFLAKE && strcmp(t->string, "<") == 0) {
        drop_token(t);
        while (t->type != SNOWFLAKE || strcmp(t->string, ">") != 0) {
            TypeValue* g = parse_type_value(stream);
            list_append(&generics, g);
            t = next_token(stream);
            if (t == NULL) unexpected_eof(stream);
            if (t->type == SNOWFLAKE && strcmp(t->string, ",") == 0) drop_token(t);
            else if (t->type != SNOWFLAKE || strcmp(t->string, ">") != 0) unexpected_token_error(t, stream);
        }
        drop_token(t);
    } else {
        stream->peek = t;
    }
    return generics;
}

TypeDef* __parse_type_def(TokenStream* stream) {
    usize start_line = stream->line;
    Token* t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER || strcmp(t->string, "type") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    str name = t->string;
    free(t);
    StrList generics = parse_generics_def(stream);
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
    StrList fields = list_new();
    TypeValueList fields_t = list_new();
    while (t->type != SNOWFLAKE || strcmp(t->string, "}") != 0) {
        if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
        str field = t->string;
        free(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        TypeValue* field_t = parse_type_value(stream);
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
    s->src_line = start_line;
    s->name = sname;
    s->fields = fields;
    s->fields_t = fields_t;
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_STRUCT;
    type->ty = s;
    TypeDef* tydef = malloc(sizeof(TypeDef));
    tydef->name = name;
    tydef->type = type;
    tydef->generics = generics;
    tydef->mappings = (GenericsMappingList)list_new();
    return tydef;
}

FunctionDef* __parse_function_def(TokenStream* stream) {
    Token* t = next_token(stream);
    usize start_line = stream->line;
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER || strcmp(t->string, "fn") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != IDENTIFIER) unexpected_token_error(t, stream);
    str name = t->string;
    free(t);
    StrList generics = parse_generics_def(stream);
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    if (t->type != SNOWFLAKE || strcmp(t->string, "(") != 0) unexpected_token_error(t, stream);
    drop_token(t);
    StrList args = list_new();
    TypeValueList args_t = list_new();
    FunctionDef* func = malloc(sizeof(FunctionDef));
    func->is_variadic = false;
    t = next_token(stream);
    if (t == NULL) unexpected_eof(stream);
    while (t->type != SNOWFLAKE || strcmp(t->string, ")") != 0) {
        if (t->type != IDENTIFIER) {
            if (t->type == SNOWFLAKE && strcmp(t->string, "*") == 0) {
                drop_token(t);
                t = next_token(stream);
                if (t == NULL) unexpected_eof(stream);
                if (t->type != SNOWFLAKE || strcmp(t->string, ")") != 0) unexpected_token_error(t, stream);
                func->is_variadic = true;
                break;
            }
            unexpected_token_error(t, stream);
        }
        list_append(&args, t->string);
        free(t);
        t = next_token(stream);
        if (t == NULL) unexpected_eof(stream);
        if (t->type != SNOWFLAKE || strcmp(t->string, ":") != 0) unexpected_token_error(t, stream);
        drop_token(t);
        TypeValue* ty = parse_type_value(stream);
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
            ret_value->expr = copy_str("unit");
            Expression* ret = malloc(sizeof(Expression));
            ret->src_line = stream->line;
            ret->kind = RETURN_EXPR;
            ret->expr = ret_value;
            list_append(&(body->exprs), ret);
        }
    } else {
        drop_token(t);
    }
    if (func->ret_t == NULL) {
        func->ret_t = new_type_value("unit", stream->line);
    }

    func->name = name;
    func->body = body;
    func->args_t = args_t;
    func->args = args;
    func->src_line = start_line;
    func->generics = generics;
    func->mappings = (GenericsMappingList)list_new();
    return func;
}

void drop_module(Module* module) {
    list_foreach(&(module->funcs), drop_function_def);
    list_foreach(&(module->types), drop_type_def);
    free(module->funcs.elements);
    free(module->types.elements);
    free(module);
}

TypeDef* find_type_def(Module* module, TypeValue* type) {
    for (usize i = 0;i < module->types.length;i++) {
        if (strcmp(type->name, module->types.elements[i]->name) == 0) {
            TypeDef* td = module->types.elements[i];
            if (td->generics.length != type->generics.length) {
                printf("Type `");
                fprint_type_value(stderr, type);
                printf("` is expected to have %lld generic parameters: <", td->generics.length);
                for (usize j = 0;j < td->generics.length;j++) {
                    if (j > 0) fprintf(stderr, ", ");
                    fprintf(stderr, "%s", td->generics.elements[i]);
                }
                printf("> but got %lld (near line %lld)\n", type->generics.length, type->src_line + 1);
                exit(1);            
            }
            if (td->generics.length > 0) {
                for (usize j = 0;j < td->mappings.length;j++) {
                    for (usize k = 0;k < td->generics.length;k++) {
                        if (!type_match(td->mappings.elements[j].elements[k]->type, type->generics.elements[k])) {
                            goto mismatch;
                        }
                    }
                    goto exists;
                    mismatch: {}
                }
                GenericsMapping mapping = list_new();
                for (usize k = 0;k < td->generics.length;k++) {
                    GenericValue* gv = malloc(sizeof(GenericValue));
                    gv->name = copy_str(td->generics.elements[k]);
                    gv->type = type->generics.elements[k];
                    list_append(&mapping, gv);
                }
                list_append(&(td->mappings), mapping);
                printf("    registered new generic variant: ");
                usize i = type->indirection;
                type->indirection = 0;
                fprint_type_value(stdout, type);
                type->indirection = i;
                printf(" for ");
                fprint_type(stdout, td);
                printf(" (%lld)\n", td->mappings.length);
                exists: {}
            }
            if (td->type->type == TYPE_STRUCT) {
                Struct* s = td->type->ty;
                for (usize j = 0;j < s->fields_t.length;j++) {
                    if (td->generics.length == 0) find_type_def(module, s->fields_t.elements[j]);
                    else find_type_def(module, degenerify(s->fields_t.elements[j], &td->generics, &type->generics, type->src_line));
                }
            }
            return td;
        }
    }
    printf("No such type `");
    fprint_type_value(stderr, type);
    printf("` (near line %lld)\n", type->src_line + 1);
    exit(1);
}

FunctionDef* find_func_def(Module* module, FunctionCall* func, usize src_line) {
    for (usize i = 0;i < module->funcs.length;i++) {
        if (strcmp(func->name, module->funcs.elements[i]->name) == 0) {
            FunctionDef* def = module->funcs.elements[i];
            if (func->generics.length != def->generics.length) {
                fprintf(stderr, "Invalid number of generics `");
                fprint_func_value(stderr, func);
                fprintf(stderr, "` (near line %lld) for `", src_line);
                fprint_func(stderr, def);
                fprintf(stderr, "` (near line %lld)\n", def->src_line);            
            }
            if (def->generics.length > 0) {
                for (usize j = 0;j < def->mappings.length;j++) {
                    for (usize k = 0;k < def->generics.length;k++) {
                        if (!type_match(def->mappings.elements[j].elements[k]->type, func->generics.elements[k])) {
                            goto mismatch;
                        }
                    }
                    goto exists; 
                    mismatch: {}
                }
                GenericsMapping mapping = list_new();
                for (usize k = 0;k < def->generics.length;k++) {
                    GenericValue* gv = malloc(sizeof(GenericValue));
                    gv->name = copy_str(def->generics.elements[k]);
                    gv->type = func->generics.elements[k];
                    list_append(&mapping, gv);
                }
                list_append(&def->mappings, mapping);
                printf("    registered new generic variant: ");
                fprint_func_value(stdout, func);
                printf("\n");
                exists: {}
            }
            return def;
        }
    }
    fprintf(stderr, "No such function `");
    fprint_func_value(stderr, func);
    fprintf(stderr, "` (near line %lld)\n", src_line);
    exit(1);
}

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
            free(module->funcs.elements);
            free(module->types.elements);
            free(module);
            printf("finished importing %s\n", fname);
            free(fname);
        } else if (strcmp(t->string, "fn") == 0) {
            stream->peek = t;
            FunctionDef* func = parse_function_def(stream);
            if (func->body == NULL) printf("    parsed function definition `");
            else printf("    parsed function `");
            fprint_func(stdout, func);
            if (func->body == NULL) printf("`\n");
            else printf("` with %lld expressions\n", func->body->exprs.length);
            list_append(&funcs, func);
        } else if (strcmp(t->string, "type") == 0) {
            stream->peek = t;
            TypeDef* tydef = parse_type_def(stream);
            printf("    parsed type `");
            fprint_type(stdout, tydef);
            printf("`\n");
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