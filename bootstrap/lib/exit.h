#ifndef LIB_ERROR_H
#define LIB_ERROR_H

#include "defines.h"

extern bool TRACE_ON_PANIC;
extern bool LOG_LINES;

void init_exit_handler(void (*exit_func)(str file, usize line, int code));

void fprint_stacktrace(FILE* file);

void __panic(str file, usize line, str fmt, ...) __attribute__ ((noreturn));
#define spanic(fmt, ...) __panic(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
void __quit(str file, usize line, int code) __attribute__ ((noreturn));
#define quit(code) __quit(__FILENAME__, __LINE__, code)

#endif