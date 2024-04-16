#pragma once

#include "lib/defines.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define unexpected_token_error(t, stream) { \
    fprintf(stderr, "Unexpected token %s `%s` in line %lld during %s@%s:%d\n", TOKENTYPE_STRING[t->type], t->string, stream->line + 1, __func__, __FILENAME__, __LINE__); \
    exit(1); \
}

#define unexpected_eof(stream) { \
    fprintf(stderr, "Unexpected EOF during %s@%s:%d", __func__, __FILENAME__, __LINE__); \
    exit(1); \
}

#define print_token(t) printf("%s: %s\n", TOKENTYPE_STRING[t->type], t->string)

typedef enum TokenType {
    STRING,
    IDENTIFIER,
    SNOWFLAKE
} TokenType;

static const char *TOKENTYPE_STRING[] = {
    "STRING", "IDENTIFIER", "SNOWFLAKE"
};

typedef struct Token {
    TokenType type;
    char* string;
} Token;

void drop_token(Token* token) {
    free(token->string);
    free(token);
}

bool is_alphabetic(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_numeric(char c) {
    return c >= '0' && c <= '9';
}

typedef struct TokenStream {
    FILE* fptr;
    char* peek_char;
    Token* peek;
    usize line;
} TokenStream;

TokenStream* new_tokenstream(FILE* fptr) {
    TokenStream* stream = malloc(sizeof(TokenStream));
    stream->line = 0;
    stream->fptr = fptr;
    stream->peek_char = malloc(sizeof(char));
    *stream->peek_char = -1;
    stream->peek = NULL;
    return stream;
}

void drop_tokenstream(TokenStream* stream) {
    fclose(stream->fptr);
    free(stream->peek_char);
    free(stream->peek);
    free(stream);
}

Token* next_token(TokenStream* stream) {
    if (stream->peek != NULL) {
        Token* t = stream->peek;
        stream->peek = NULL;
        return t;
    }
    static char next_token_buffer[32];
    TokenType type;
    usize len = 0;
    usize tok_len = 0;
    char* tok = NULL;
    char next = 0;
    bool in_str = false;
    while (tok_len == 0 && next != -1) {
        while (1) {
            if (*stream->peek_char != -1) next = *stream->peek_char;
            else next = fgetc(stream->fptr);
            if (next == '\n') stream->line += 1;
            *stream->peek_char = -1;
            if (next == '"') {
                in_str = !in_str;
                if (!in_str) {
                    type = STRING;
                    break;
                } else {
                    if (tok_len > 0) {
                        *stream->peek_char = next; 
                        type = IDENTIFIER; 
                        break;
                    }
                }
                continue;
            } else if(!in_str) {
                // obvious terminators
                if (next == -1 || next == ' ' || next == '\n' || next == '\t' || next == '\r') { type = IDENTIFIER; break; }
                // word end and special char now
                if (tok_len > 0 && !is_alphabetic(next) && !is_numeric(next)) { *stream->peek_char = next; type = IDENTIFIER; break; };
            }
            next_token_buffer[len] = next;
            len += 1;
            tok_len += 1;
            // single special char
            if (!in_str && !is_alphabetic(next) && !is_numeric(next)) { type = SNOWFLAKE; break; }
            if (len >= 32) {
                tok = (char*) realloc(tok, tok_len + len + 1);
                memcpy(tok + tok_len - len, &next_token_buffer, len);
                len = 0;
            }
        }
    }
    if (tok_len == 0) return NULL;

    tok = (char*) realloc(tok, tok_len + len + 1);
    memcpy(tok + tok_len - len, &next_token_buffer, len);

    tok[tok_len] = 0;

    Token* t = (Token*) malloc(sizeof(Token));
    t->string = tok;
    t->type = type;
    return t;
}