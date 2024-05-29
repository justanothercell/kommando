#include "tokens.h"

#include "lib/defines.h"

#include "lib/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* TOKENTYPE_STRING[] = {
    "STRING", "NUMERAL", "IDENTIFIER", "SNOWFLAKE"
};

bool is_alphabetic(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_numeric(char c) {
    return c >= '0' && c <= '9';
}

TokenStream* new_tokenstream(FILE* fptr) {
    TokenStream* stream = malloc(sizeof(TokenStream));
    stream->line = 0;
    stream->fptr = fptr;
    stream->peek_char = malloc(sizeof(char));
    *stream->peek_char = -1;
    stream->peek = NULL;
    stream->column = 0;
    return stream;
}

void drop_tokenstream(TokenStream* stream) {
    fclose(stream->fptr);
    free(stream->peek_char);
    free(stream->peek);
    free(stream);
}

Token* __next_token(TokenStream* stream) {
    if (stream->peek != NULL) {
        Token* t = stream->peek;
        stream->peek = NULL;
        return t;
    }
    TokenType type;
    usize tok_len = 0;
    usize tok_cap = 8;
    str tok = malloc(tok_cap * sizeof(char));
    char next = 0;
    bool in_str = false;
    while (tok_len == 0 && next != -1) {
        while (1) {
            if (*stream->peek_char != -1) next = *stream->peek_char;
            else next = fgetc(stream->fptr);
            stream->column += 1;
            if (next == '\n') { stream->line += 1; stream->column = 0; }
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
                if (tok_len > 0 && !is_alphabetic(next) && !is_numeric(next) && next != '_') { *stream->peek_char = next; type = IDENTIFIER; break; };
            }
            list_append_raw(next, tok, tok_len, tok_cap);
            // single special char
            if (!in_str && !is_alphabetic(next) && !is_numeric(next) && next != '_') { type = SNOWFLAKE; break; }
        }
    }
    if (tok_len == 0) return NULL;

    list_append_raw('\0', tok, tok_len, tok_cap);

    Token* t = (Token*) malloc(sizeof(Token));
    t->string = tok;
    if (type == IDENTIFIER && is_numeric(*t->string)) t->type = NUMERAL;
    else t->type = type;
    return t;
}