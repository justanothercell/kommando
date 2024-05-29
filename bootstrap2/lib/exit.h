#ifndef LIB_ERROR_H
#define LIB_ERROR_H

#include "defines.h"

void init_exit_handler(void (*exit_func)(str file, usize line, int code));

void __panic(str file, usize ine, str fmt, ...);
#define panic(fmt, ...) __panic(__FILENAME__, __LINE__, fmt, ## __VA_ARGS__)

void __quit(str file, usize line, int code);
#define quit(code) __quit(__FILENAME__, __LINE__, code)

#endif