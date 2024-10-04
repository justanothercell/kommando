#include "parser.h"
#include "ast.h"
#include "lib.h"
#include "lib/exit.h"
#include "lib/gc.h"
#include "lib/list.h"
#include "lib/str.h"
#include "module.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>

bool double_double_colon(TokenStream* stream) {
    Token* t = try_next_token(stream);
    if (t == NULL) return false;
    if (!token_compare(t, ":", SNOWFLAKE)) {
        stream->peek = t;
        return false;
    }
    t = next_token(stream);
    if (!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t);
    return true;
}

Module* parse_module_contents(TokenStream* stream, Path* path) {
    Module* module = gc_malloc(sizeof(Module));
    module->imports = list_new(PathList);
    module->functions = map_new();
    module->types = map_new();
    module->path = path;
    module->resolved = false;

    while (has_next(stream)) {
        Token* t = next_token(stream);
        stream->peek = t;
        if (token_compare(t, "fn", IDENTIFIER)) {
            FuncDef* function = parse_function_definition(stream);
            map_put(module->functions, function->name->name, function);
        } else if (token_compare(t, "type", IDENTIFIER)) {
            todo("Type defintition parsing");
        } else {
            unexpected_token(t);
        }
    }
    return module;
}

Expression* parse_expresslet(TokenStream* stream) {
    Expression* expression = gc_malloc(sizeof(Expression));
    Token* t = next_token(stream);
    CodePoint start = t->span.left;
    CodePoint end = t->span.right;
    
    if (token_compare(t, "let", IDENTIFIER)) {
        Identifier* name = parse_identifier(stream);
        end = name->span.right;
        LetExpr* let = gc_malloc(sizeof(LetExpr));
        Variable* var = gc_malloc(sizeof(Variable));
        var->id = 0;
        var->name = name;
        let->var = var;
        t = next_token(stream);
        if (token_compare(t, "=", SNOWFLAKE)) {
            let->value = parse_expression(stream);
            end = let->value->span.right;
        } else {
            let->value = NULL;
            stream->peek = t;
        }
        let->type = NULL;
        expression->expr = let;
        expression->type = EXPR_LET;
    } else if (token_compare(t, "return", IDENTIFIER)) {
        t = next_token(stream);
        stream->peek = t;
        if (token_compare(t, ";", SNOWFLAKE)) {
            expression->expr = NULL;
        } else {
            expression->expr = parse_expression(stream);
            end = ((Expression*)expression->expr)->span.right;
        }
        expression->type = EXPR_RETURN;
    } else if (token_compare(t, "break", IDENTIFIER)) {
        t = next_token(stream);
        stream->peek = t;
        if (token_compare(t, ";", SNOWFLAKE)) {
            expression->expr = NULL;
        } else {
            expression->expr = parse_expression(stream);
            end = ((Expression*)expression->expr)->span.right;
        }
        expression->type = EXPR_BREAK;
    } else if (token_compare(t, "contimue", IDENTIFIER)) {
        expression->type = EXPR_CONTINUE;
        expression->expr = NULL;
    } else if (token_compare(t, "if", IDENTIFIER)) {
        Conditional* conditional = gc_malloc(sizeof(Conditional));
        conditional->cond = parse_expression(stream);
        conditional->then = parse_block(stream);
        end = conditional->then->span.right;
        t = next_token(stream);
        if (token_compare(t, "else", IDENTIFIER)) {
            conditional->otherwise = parse_block(stream);
        } else {
            conditional->otherwise = NULL;
            stream->peek = t;
        }
        expression->expr = conditional;
        expression->type = EXPR_CONDITIONAL;
    } else if (token_compare(t, "while", IDENTIFIER)) {
        WhileLoop* while_loop = gc_malloc(sizeof(WhileLoop));
        while_loop->cond = parse_expression(stream);
        while_loop->body = parse_block(stream);
        end = while_loop->body->span.right;
        expression->expr = while_loop;
        expression->type = EXPR_WHILE_LOOP;
    } else if (t->type == IDENTIFIER || token_compare(t, ":", SNOWFLAKE)) {
        stream->peek = t;
        Path* path = parse_path(stream);
        if (path->elements.length > 0) end = path->elements.elements[path->elements.length - 1]->span.right;
        t = next_token(stream);
        if (token_compare(t, "(", SNOWFLAKE)) {
            ExpressionList arguments = list_new(ExpressionList); 
            while (true) {
                Expression* argument = parse_expression(stream);
                list_append(&arguments, argument);
                t = next_token(stream);
                if (token_compare(t, ")", SNOWFLAKE)) {
                    end = t->span.right;
                    break;
                } else if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
            }
            FuncCall* call = gc_malloc(sizeof(FuncCall));
            call->name = path;
            call->arguments = arguments;
            expression->expr = call;
            expression->type = EXPR_FUNC_CALL;            
        } else {
            if (path->absolute || path->elements.length != 1) panic("This path is not a single variable: %s", to_str_writer(stream, fprint_path(stream, path)));
            stream->peek = t;
            Variable* var = gc_malloc(sizeof(Variable));
            var->id = 0;
            var->name = path->elements.elements[0];
            expression->expr = var;
            expression->type = EXPR_VARIABLE;
        }
    } else if (t->type == NUMERAL || t->type == STRING) {
        expression->type = EXPR_LITERAL;
        expression->expr = t;
    } else unexpected_token(t);

    expression->span = from_points(&start, &end);
    return expression;
}

int bin_op_precedence(str op) {
    if (str_eq("||", op)) return 0;
    if (str_eq("&&", op)) return 1;
    if (str_eq("==", op) || str_eq("!=", op) || str_eq(">", op) 
    || str_eq("<", op) || str_eq(">=", op) || str_eq("<=", op)) return 2;
    if (str_eq("|", op)) return 3;
    if (str_eq("^", op)) return 4;
    if (str_eq("&", op)) return 5;
    if (str_eq(">>", op) || str_eq("<<", op)) return 6;
    if (str_eq("+", op) || str_eq("-", op)) return 7;
    if (str_eq("*", op) || str_eq("/", op) || str_eq("%", op)) return 8;
    unreachable("Invalid binop %s", op);
}

Expression* parse_expression(TokenStream* stream) {
    Expression* expr = parse_expresslet(stream);
    while (true) {
        Token* t = next_token(stream);
        if (t->type == SNOWFLAKE) {
            if (str_eq("+", t->string) || str_eq("-", t->string) || str_eq("*", t->string) || str_eq("/", t->string)
             || str_eq("|", t->string) || str_eq("&", t->string) || str_eq("^", t->string) || str_eq("!", t->string)
             || str_eq("%", t->string) || str_eq(">", t->string) || str_eq("<", t->string) || str_eq("=", t->string))    {
                Token* n = next_token(stream);
                if (n->type == SNOWFLAKE) {
                    str s = t->string;
                    t->string = gc_malloc(3);
                    t->string[0] = s[0];
                    t->string[1] = n->string[0];
                    t->string[2] = 0;
                    t->span.right = n->span.right;
                } else {
                    if (str_eq("=", t->string)) unexpected_token(n);
                    stream->peek = n;
                }
                Expression* rhs = parse_expresslet(stream);
                if (expr->type == EXPR_BIN_OP) {
                    BinOp* lhs = expr->expr;
                    if (bin_op_precedence(lhs->op) < bin_op_precedence(t->string)) {
                        Expression* a = lhs->lhs;
                        Expression* b = lhs->rhs;
                        Expression* c = rhs;
                        BinOp* op = gc_malloc(sizeof(BinOp));
                        op->lhs = b;
                        op->rhs = c;
                        op->op = t->string;
                        op->op_span = t->span;
                        t->string = lhs->op;
                        t->span = lhs->op_span;
                        Expression* op_expr = gc_malloc(sizeof(Expression));
                        op_expr->expr = op;
                        op_expr->type = EXPR_BIN_OP;
                        op_expr->span = from_points(&op->lhs->span.left, &op->rhs->span.right);
                        expr = a;
                        rhs = op_expr;
                    }
                }
                BinOp* op = gc_malloc(sizeof(BinOp));
                op->lhs = expr;
                op->rhs = rhs;
                op->op = t->string;
                op->op_span = t->span;
                Expression* parent = gc_malloc(sizeof(Expression));
                parent->expr = op;
                parent->type = EXPR_BIN_OP;
                parent->span = from_points(&op->lhs->span.left, &op->rhs->span.right);
                expr = parent;
            } else {
                stream->peek = t;
                return expr;
            }
        } else {
            stream->peek = t;
            return expr;
        }
    }
}

Path* parse_path(TokenStream* stream) {
    bool absolute = double_double_colon(stream);
    IdentList elements = list_new(IdentList);
    do {
        list_append(&elements, parse_identifier(stream));
    } while (double_double_colon(stream));
    return path_new(absolute, elements);
}

TypeValue* parse_type_value(TokenStream* stream) {
    Path* name = parse_path(stream);
    TypeValue* tval = gc_malloc(sizeof(TypeValue));
    tval->name = name;
    tval->generics = list_new(TypeValueList);
    tval->def = NULL;
    Token* t = try_next_token(stream);
    if (t != NULL) {
        if (token_compare(t, "<", SNOWFLAKE)) {
            t = next_token(stream);
            if (!token_compare(t, ">", SNOWFLAKE)) {
                stream->peek = t;
                while (true) {
                    TypeValue* g = parse_type_value(stream);
                    list_append(&tval->generics, g);
                    t = next_token(stream);
                    if (token_compare(t, ">", SNOWFLAKE)) break;
                    if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
                }
            }
        } else {
            stream->peek = t;
        }
    }
    return tval;
}

Identifier* parse_identifier(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t->type != IDENTIFIER) unexpected_token(t);
    Identifier* identifier = gc_malloc(sizeof(Identifier));
    identifier->name = t->string;
    identifier->span = t->span;;
    return identifier;
}

Block* parse_block(TokenStream* stream) {
    ExpressionList expressions = list_new(ExpressionList);
    bool yield_last = false;
    
    Token* t = next_token(stream);
    if (!token_compare(t, "{", SNOWFLAKE)) unexpected_token(t);
    CodePoint start = t->span.left;
    CodePoint end;

    while (true) {
        Expression* expression = parse_expression(stream);
        list_append(&expressions, expression);
        t = next_token(stream);
        if (!token_compare(t, ";", SNOWFLAKE)) {
            if (token_compare(t, "}", SNOWFLAKE)) {
                yield_last = true;
                end = t->span.right;
                break;
            } else unexpected_token(t);
        }
        t = next_token(stream);
        if (token_compare(t, "}", SNOWFLAKE)) {
            end = t->span.right;
            break;
        }
        stream->peek = t;
    }

    Block* block = gc_malloc(sizeof(Block));
    block->expressions = expressions;
    block->yield_last = yield_last;
    block->span = from_points(&start, &end);
    return block;
}

FuncDef* parse_function_definition(TokenStream* stream) {
    Token* t = next_token(stream);
    if (!token_compare(t, "fn", IDENTIFIER)) unexpected_token(t);
    Identifier* name = parse_identifier(stream);
    t = next_token(stream);
    if (!token_compare(t, "(", SNOWFLAKE)) unexpected_token(t);
    t = next_token(stream);
    ArgumentList arguments = list_new(ArgumentList); 
    if (!token_compare(t, ")", SNOWFLAKE)) {
        stream->peek = t;
        while (true) {
            Argument* argument = gc_malloc(sizeof(Argument));
            Variable* var = gc_malloc(sizeof(Variable));
            var->id = 0;
            var->name = parse_identifier(stream);
            argument->var = var;
            t = next_token(stream);
            if (!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t);
            argument->type = parse_type_value(stream);
            list_append(&arguments, argument);
            t = next_token(stream);
            if (token_compare(t, ")", SNOWFLAKE)) {
                break;
            } else if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
        }
    }
    TypeValue* return_type = NULL;
    t = next_token(stream);
    if (!token_compare(t, "{", SNOWFLAKE)) {
        if (!token_compare(t, "-", SNOWFLAKE))  unexpected_token(t);
        t = next_token(stream);
        if (!token_compare(t, ">", SNOWFLAKE))  unexpected_token(t);
        return_type = parse_type_value(stream);
    } else {
        stream->peek = t;
    }
    Block* body = parse_block(stream);
    FuncDef* func = gc_malloc(sizeof(FuncDef));
    func->body = body;
    func->name = name;
    func->return_type = return_type;
    func->args = arguments;
    func->is_variadic = false;
    return func;
}