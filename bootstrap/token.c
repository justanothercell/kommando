#include "token.h"
#include "lib.h"
#include "lib/exit.h"
#include "lib/str.h"
#include "parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool is_alphabetic(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_numeric(char c) {
    return c >= '0' && c <= '9';
}

bool is_whitespace(char c) {
    return str_contains_char(" \n\r\t", c);
}

char next_char(TokenStream* stream) {
    if (stream->peek_char !='\0') {
        char next = stream->peek_char;
        stream->peek_char = '\0';
        return next;
    }
    if (stream->point.index >= stream->length) {
        if (stream->last) return '\0';
        stream->last = true;
        return ' ';
    }
    char next = stream->point.source[stream->point.index];
    stream->point.index += 1;
    stream->point.column += 1;
    stream->peek_char = '\0';
    if (next == '\n') { stream->point.line += 1; stream->point.column = 0; }
    return next;
}

Token* try_next_token(TokenStream* stream) {
    if (stream->ppeek != NULL) {
        if (stream->peek == NULL) panic("Compiler error: ppeek was available but peek was null @ %s", to_str_writer(s, fprint_span(s, &stream->ppeek->span)));
        Token* t = stream->ppeek;
        stream->ppeek = NULL;
        return t;
    }
    if (stream->peek != NULL) {
        Token* t = stream->peek;
        stream->peek = NULL;
        return t;
    }
    TokenType type = IDENTIFIER;
    String tok = list_new(String);
    char next = '\0';
    CodePoint start;
    while (true) {
        start = stream->point;
        next = next_char(stream);
        if (next == '\0') goto eof;
        if (is_whitespace(next)) { 
            start = stream->point;
            continue;
        }
        if (next == '/') {
            next = next_char(stream);
            if (next == '\0') goto eof;
            if (next == '/') {
                while (true) {
                    next = next_char(stream);
                    if (next == '\0') goto eof;
                    if (next == '\n') break;
                }
                continue;
            }
            if (next == '*') {
                while (true) {
                    next = next_char(stream);
                    if (next == '\0') goto eof;
                    if (next == '*') {
                        next = next_char(stream);
                        if (next == '\0') goto eof;
                        if (next == '/') break;
                    }
                }
                continue;
            }
            stream->peek_char = next;
            next = '/';
        }
        break;
    }
    if (next == '"') {
        type = STRING;
        while (true) {
            next = next_char(stream);
            if (next == '\0') goto eof;
            if (next == '\\') {
                next = next_char(stream);
                if (next == '"') list_append(&tok, '"');
                else if (next == '\\') list_append(&tok, '\\');
                else if (next == 'n') list_append(&tok, '\n');
                else if (next == 'r') list_append(&tok, '\r');
                else if (next == 't') list_append(&tok, '\t');
                else if (next == 'x') {
                    char a = next_char(stream);
                    if (!str_contains_char("1234567890abcdefABCDEF", a)) {
                        Span here = from_points(&stream->point, &stream->point);
                        spanned_error("Invalid hex digit", here, "Invalid hex digit for string escape \\x%c%c", a, next_char(stream));
                    }
                    char b = next_char(stream);
                    if (!str_contains_char("1234567890abcdefABCDEF", b)) {
                        Span here = from_points(&stream->point, &stream->point);
                        spanned_error("Invalid hex digit", here, "Invalid hex digit for string escape \\x%c%c", a, b);
                    }
                    char str[] = { a, b, '\0' };
                    unsigned char from_hex = strtoul(str, NULL, 16);
                    list_append(&tok, *(char*)&from_hex);
                } else {
                    Span here = from_points(&stream->point, &stream->point);
                    spanned_error("Invalid string escape", here, "No such escape sequence \\%c", next);
                }
            } else {
                if (next == '"') break;
                list_append(&tok, next);
            }
        }
    } else if (is_alphabetic(next) || is_numeric(next) || next == '_') {
        list_append(&tok, next);
        bool dotted = false;
        if (is_numeric(next)) type = NUMERAL;
        else type = IDENTIFIER;
        while (true) {
            next = next_char(stream);
            if (next == '\0') goto eof;
            if (is_alphabetic(next) || is_numeric(next) || next == '_' || (next == '.' && type == NUMERAL && !dotted && (stream->point.index >= stream->length || is_numeric(stream->point.source[stream->point.index])))) {
                if (next == '.') dotted = true;
                if (type == IDENTIFIER || next != '_') list_append(&tok, next);
            } else break;
        }
        stream->peek_char = next;
    } else {
        list_append(&tok, next);
        type = SNOWFLAKE;
    }
    eof:
    if (tok.length == 0 && type != STRING) return NULL;
    list_append(&tok, '\0');

    Token* t = malloc(sizeof(Token));
    t->string = tok.elements;
    t->span = from_points(&start, &stream->point);
    t->type = type;
    return t;
}

Token* next_token(TokenStream* stream) {
    Token* t = try_next_token(stream);
    if (t == NULL) panic("Unexpected EOF in `%s`", stream->point.file);
    return t;
}

Token* peek_next_token(TokenStream* stream) {
    if (stream->ppeek != NULL) return stream->ppeek;
    if (stream->peek != NULL) return stream->peek;
    Token* t = try_next_token(stream);
    if (t == NULL) return NULL;
    stream->peek = t;
    return stream->peek;
}

bool has_next(TokenStream* stream) {
    if (stream->ppeek != NULL) return true;
    if (stream->peek != NULL) return true;
    Token* t = try_next_token(stream);
    if (t == NULL) return false;
    stream->peek = t;
    return true;
}

TokenStream* tokenstream_new(str file, str source) {
    TokenStream* stream = malloc(sizeof(TokenStream));

    stream->peek = NULL;
    stream->ppeek = NULL;
    stream->peek_char = '\0';
    stream->length = strlen(source);
    stream->point.source = source;
    stream->point.file = file;
    stream->point.line = 0;
    stream->point.column = 0;
    stream->point.index = 0;
    stream->last = false;

    return stream;
}

void fprint_token(FILE* file, Token* t) {
    if(t->type == STRING) fprintf(file, "\"%s\"", t->string);
    else fprintf(file, "%s", t->string);
}

Span from_points(CodePoint* left, CodePoint* right) {
    if (left->source != right->source) panic("Left and right codepoint have to point to the same source code: %p in %s and %p in %s", left->source, left->file, right->source, right->file);
    return (Span) { *left, *right };
}

void fprint_span(FILE* file, Span* span) {
    if (span == NULL) {
        fprintf(file, "(null):0:0-0");
        return;
    }
    if (span->left.index + 1 == span->right.index) fprintf(file, "%s:%lld:%lld-%lld", span->left.file, span->left.line + 1, span->left.column+1, span->left.column+2);
    else if (span->left.line == span->right.line) fprintf(file, "%s:%lld:%lld-%lld", span->left.file, span->left.line + 1, span->left.column + 1, span->right.column);
    else fprintf(file, "%s:%lld:%lld", span->left.file, span->left.line + 1, span->left.column);
}

void fprint_span_contents(FILE* file, Span* span) {
    if (span == NULL) {
        fprintf(file, "(null)");
        return;
    }
    fprintf(file, "%.*s", (int)(span->right.index - span->left.index), span->left.source + span->left.index);
}

bool token_compare(Token* t, str string, TokenType type) {
    return str_eq(t->string, string) && t->type == type;
}