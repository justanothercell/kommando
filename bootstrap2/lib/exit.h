#ifndef LIB_ERROR_H
#define LIB_ERROR_H

#include "defines.h"

void init_exit_handler(void (*exit_func)(str file, usize line, int code));

void fprint_stacktrace(FILE* file);

void __panic(str file, usize line, str fmt, ...);
#define panic(fmt, ...) __panic(__FILENAME__, __LINE__, fmt, ## __VA_ARGS__)
#define unreachable(...) panic("Unreachabe expression reached" __VA_ARGS__)
#define todo(...) panic("TODO: " __VA_ARGS__)

void __quit(str file, usize line, int code);
#define quit(code) __quit(__FILENAME__, __LINE__, code)

#endif