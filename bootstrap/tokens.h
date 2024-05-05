#ifndef TOKENS_H
#define TOKENS_H

#include "lib/defines.h"
#include "lib/list.h"
#include <stdio.h>

#define unexpected_token_error(t, stream) { \
    fprintf(stderr, "Unexpected token %s `%s` in line %lld column %lld during %s %s:%d\n", TOKENTYPE_STRING[t->type], t->string, stream->line + 1, stream->column, __func__, __FILENAME__, __LINE__); \
    exit(1); \
}

#define unexpected_eof(stream) { \
    fprintf(stderr, "Unexpected EOF during %s@%s:%d", __func__, __FILENAME__, __LINE__); \
    exit(1); \
}

#define print_token(t) printf("%s: %s\n", TOKENTYPE_STRING[t->type], t->string)

typedef enum TokenType {
    STRING,
    NUMERAL,
    IDENTIFIER,
    SNOWFLAKE
} TokenType;

static const char *TOKENTYPE_STRING[4];

typedef struct Token {
    TokenType type;
    str string;
} Token;

#define drop_token(token) ({ \
    free((token)->string); \
    free(token); \
})

bool is_alphabetic(char c);

bool is_numeric(char c);

typedef struct TokenStream {
    FILE* fptr;
    str peek_char;
    Token* peek;
    usize line;
    usize column;
} TokenStream;

TokenStream* new_tokenstream(FILE* fptr);

void drop_tokenstream(TokenStream* stream);

#ifdef __TRACE__
    #define next_token(stream) ({ \
        Token* t = __next_token(stream); \
        for (int i = 0;i < trace_indent;i++) { printf("| "); } \
        if (t == NULL) printf("*-token: %s %s:%d EOF\n", __func__, __FILENAME__, __LINE__); \
        else printf("*-token: %s %s:%d %s: %s in line %lld column %lld\n", __func__, __FILENAME__, __LINE__, TOKENTYPE_STRING[t->type], t->string, stream->line + 1, stream->column); \
        t; \
    })
#else
    #define next_token(stream) __next_token(stream)
#endif


Token* __next_token(TokenStream* stream);

#endif