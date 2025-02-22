#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lib.h"
#include "module.h"
#include "token.h"

extern Expression* CURRENT_EXPR;
#define panic(fmt, ...) do { \
    if (CURRENT_EXPR == NULL) { \
        __panic(__FILENAME__, __LINE__, fmt, ## __VA_ARGS__); \
    } else { \
        if (CURRENT_EXPR->span.right.line > CURRENT_EXPR->span.left.line) { \
            while (CURRENT_EXPR->span.right.line > CURRENT_EXPR->span.left.line) { \
                CURRENT_EXPR->span.right.index -= 1; \
                if (CURRENT_EXPR->span.right.source[CURRENT_EXPR->span.right.index] == '\n') { \
                    CURRENT_EXPR->span.right.line -= 1; \
                } \
            } \
            CURRENT_EXPR->span.right.column = CURRENT_EXPR->span.left.column + (CURRENT_EXPR->span.right.index - CURRENT_EXPR->span.left.index); \
        } \
        usize il1234567890_2 = CURRENT_EXPR->span.left.index - CURRENT_EXPR->span.left.column;\
        usize ir1234567890_2 = CURRENT_EXPR->span.right.index; \
        while (ir1234567890_2 < strlen(CURRENT_EXPR->span.right.source) && CURRENT_EXPR->span.right.source[ir1234567890_2] != '\n') ir1234567890_2 += 1; \
        char* padding1234567890_2 = malloc(CURRENT_EXPR->span.left.column+1); \
        for (usize i = 0;i < CURRENT_EXPR->span.left.column; i++) padding1234567890_2[i] = '-'; \
        if (CURRENT_EXPR->span.left.column == CURRENT_EXPR->span.right.column && CURRENT_EXPR->span.left.column > 0) padding1234567890_2[CURRENT_EXPR->span.left.column - 1] = '\0'; \
        else padding1234567890_2[CURRENT_EXPR->span.left.column] = '\0'; \
        char* indicator1234567890_2 = malloc(CURRENT_EXPR->span.right.column - CURRENT_EXPR->span.left.column + 1); \
        indicator1234567890_2[0] = '^'; \
        for (usize i = 0;i < CURRENT_EXPR->span.right.column - CURRENT_EXPR->span.left.column; i++) indicator1234567890_2[i] = '^'; \
        if (CURRENT_EXPR->span.left.column == CURRENT_EXPR->span.right.column) indicator1234567890_2[1] = '\0'; \
        else indicator1234567890_2[CURRENT_EXPR->span.right.column - CURRENT_EXPR->span.left.column] = '\0'; \
        if (CURRENT_EXPR->span.right.column != CURRENT_EXPR->span.left.column + 1) indicator1234567890_2[CURRENT_EXPR->span.right.column - CURRENT_EXPR->span.left.column - 1] = '\0'; \
        __panic(__FILENAME__, __LINE__, fmt "\nWhile resolving %s\n  %s\n % 3llu | %.*s\n     *-%s%s", ## __VA_ARGS__, \
            ExprType__NAMES[CURRENT_EXPR->type], to_str_writer(out, fprint_span(out, &CURRENT_EXPR->span)), \
            CURRENT_EXPR->span.left.line + 1, ir1234567890_2 - il1234567890_2, CURRENT_EXPR->span.left.source+il1234567890_2, padding1234567890_2, indicator1234567890_2 \
        ); \
    } \
} while (0)

#define unreachable(...) panic("Unreachabe expression reached: " __VA_ARGS__)
#define todo(...) panic("TODO: " __VA_ARGS__)

#define spanned_error(title, span1234567890, message, ...) do { \
    if (span1234567890.right.line > span1234567890.left.line) { \
        while (span1234567890.right.line > span1234567890.left.line) { \
            span1234567890.right.index -= 1; \
            if (span1234567890.right.source[span1234567890.right.index] == '\n') { \
                span1234567890.right.line -= 1; \
            } \
        } \
        span1234567890.right.column = span1234567890.left.column + (span1234567890.right.index - span1234567890.left.index); \
    } \
    usize il1234567890 = span1234567890.left.index - span1234567890.left.column;\
    usize ir1234567890 = span1234567890.right.index; \
    while (ir1234567890 < strlen(span1234567890.right.source) && span1234567890.right.source[ir1234567890] != '\n') ir1234567890 += 1; \
    char* padding1234567890 = malloc(span1234567890.left.column+1); \
    for (usize i = 0;i < span1234567890.left.column; i++) padding1234567890[i] = '-'; \
    if (span1234567890.left.column == span1234567890.right.column && span1234567890.left.column > 0) padding1234567890[span1234567890.left.column - 1] = '\0'; \
    else padding1234567890[span1234567890.left.column] = '\0'; \
    char* indicator1234567890 = malloc(span1234567890.right.column - span1234567890.left.column + 1); \
    indicator1234567890[0] = '^'; \
    for (usize i = 0;i < span1234567890.right.column - span1234567890.left.column; i++) indicator1234567890[i] = '^'; \
    if (span1234567890.left.column == span1234567890.right.column) indicator1234567890[1] = '\0'; \
    else indicator1234567890[span1234567890.right.column - span1234567890.left.column] = '\0'; \
    if (span1234567890.right.column != span1234567890.left.column + 1) indicator1234567890[span1234567890.right.column - span1234567890.left.column - 1] = '\0'; \
    panic(title ": " message "\n  %s\n % 3llu | %.*s\n     *-%s%s", \
        ##__VA_ARGS__, to_str_writer(out, fprint_span(out, &span1234567890)), \
        span1234567890.left.line + 1, ir1234567890 - il1234567890, span1234567890.left.source+il1234567890, padding1234567890, indicator1234567890 \
    ); \
} while (0)
#define unexpected_token(t, ...) spanned_error("Unexpected token", t->span, "`%s` %s. " __VA_ARGS__ , to_str_writer(out, fprint_token(out, t)), TokenType__NAMES[t->type])

Module* parse_module_contents(TokenStream* stream, Path* path);
Expression* parse_expression(TokenStream* stream, bool allow_lit);
Path* parse_path(TokenStream* stream);
TypeValue* parse_type_value(TokenStream* stream);
Identifier* parse_identifier(TokenStream* stream);
FuncDef* parse_function_definition(TokenStream* stream);
Block* parse_block(TokenStream* stream);
TypeDef* parse_struct(TokenStream* stream);
GenericKeys* parse_generic_keys(TokenStream* stream);
GenericValues* parse_generic_values(TokenStream* stream);

#endif