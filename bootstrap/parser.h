#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lib.h"
#include "module.h"
#include "token.h"

#define spanned_error(title, span, message, ...) do { \
    if (span.left.line == span.right.line) { \
        usize il1234567890 = span.left.index - span.left.column;\
        usize ir1234567890 = span.right.index; \
        while (ir1234567890 < strlen(span.right.source) && span.right.source[ir1234567890] != '\n') ir1234567890 += 1; \
        char* padding1234567890 = malloc(span.left.column+1); \
        for (usize i = 0;i < span.left.column; i++) padding1234567890[i] = '-'; \
        if (span.left.column == span.right.column && span.left.column > 0) padding1234567890[span.left.column - 1] = '\0'; \
        else padding1234567890[span.left.column] = '\0'; \
        char* indicator1234567890 = malloc(span.right.column - span.left.column + 1); \
        indicator1234567890[0] = '^'; \
        for (usize i = 0;i < span.right.column - span.left.column; i++) indicator1234567890[i] = '^'; \
        if (span.left.column == span.right.column) indicator1234567890[1] = '\0'; \
        else indicator1234567890[span.right.column - span.left.column] = '\0'; \
        if (span.right.column != span.left.column + 1) indicator1234567890[span.right.column - span.left.column - 1] = '\0'; \
        panic(title ": " message "\n  %s\n % 3llu | %.*s\n     *-%s%s", \
            ##__VA_ARGS__, to_str_writer(out, fprint_span(out, &span)), \
            span.left.line + 1, ir1234567890 - il1234567890, span.left.source+il1234567890, padding1234567890, indicator1234567890 \
        ); \
    } else { \
        panic(title ": " message " @ %s", ##__VA_ARGS__, to_str_writer(out, fprint_span(out, &span))); \
    } \
} while (0)
#define unexpected_token(t, ...) spanned_error("Unexpected token", t->span, "`%s` %s." __VA_ARGS__ , to_str_writer(out, fprint_token(out, t)), TokenType__NAMES[t->type])

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