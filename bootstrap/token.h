#ifndef TOKEN_H
#define TOKEN_H

#include "lib.h"

ENUM(TokenType,
    STRING,
    NUMERAL,
    IDENTIFIER,
    SNOWFLAKE
);

typedef struct CodePoint {
    usize line;    
    usize column;    
    usize index;
    str file;
    str source; 
} CodePoint;

typedef struct Span {
    CodePoint left;
    CodePoint right; 
} Span;
void fprint_span(FILE* file, Span* span);
void fprint_span_contents(FILE* file, Span* span);

typedef struct Token {
    TokenType type;
    str string;
    Span span;
} Token;

typedef struct TokenStream {
    char peek_char;
    Token* peek;
    usize length;
    CodePoint point;
} TokenStream;

TokenStream* tokenstream_new(str file, str source);

Span from_points(CodePoint* left, CodePoint* right);

Token* try_next_token(TokenStream* stream);
Token* next_token(TokenStream* stream);
bool has_next(TokenStream* stream);
void fprint_token(FILE* file, Token* t);
bool token_compare(Token* token, str string, TokenType type);

#endif