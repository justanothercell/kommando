#include "../lib.h"

#include <stdarg.h>
#include <stdio.h>

static void (*EXIT_FUNC)(str file, usize line, int code) = NULL;

void init_exit_handler(void (*exit_func)(str file, usize line, int code)) {
    if (EXIT_FUNC != NULL) panic("Exit handler may not be initialized twice");
    EXIT_FUNC = exit_func;
}

void __panic(str file, usize line, str fmt, ...) {
    fprint_prefix_raw(stderr, file, (int)line);
    fprintf(stderr, "Panic: ");
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    __quit(file, line, 1);
}

void __quit(str file, usize line, int code) {
    EXIT_FUNC(file, line, code);
    exit(code);
}