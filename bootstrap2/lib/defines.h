#ifndef LIB_DEFINES_H
#define LIB_DEFINES_H

#include <stdbool.h>
#include <stdio.h>

#include "terminal.h"

#define IDENTITY_MACRO(x) x
#define STRINGIFY(x) #x


#ifndef __INTELLISENSE__
#define lambda(ret_type, _body) ({ ret_type f1234567890 _body; f1234567890; })
#else
// dummy since intellij doesnt know inner functions
#define lambda(ret_type, _body) ({ void* f1234567890(); f1234567890; })
#endif

#ifdef __WIN32__
    #define __PATH_SEP__ '\\'
#else
    #define __PATH_SEP__ '/'
#endif

#define __FILENAME__ (strrchr(__FILE__, __PATH_SEP__) ? strrchr(__FILE__, __PATH_SEP__) + 1 : __FILE__)

#define fprint_prefix_raw(file, srcfile, line) fprintf(file, "[" ANSI(ANSI_BOLD,ANSI_MAGENTA_FG)"%s:%i" ANSI_RESET_SEQUENCE"] ", srcfile, line)
#define fprint_prefix(file) fprint_prefix_raw(file, __FILENAME__, __LINE__)
#define print_prefix() fprint_prefix(stdout)
#define flog(file, fmt, ...) do { fprint_prefix(file); fprintf(file, fmt, ## __VA_ARGS__); fprintf(file, "\n"); } while(0)
#define log(fmt, ...) flog(stdout, fmt, ## __VA_ARGS__)
#define finfo(file, module, fmt, ...) do { fprint_prefix(file); fprintf(file, "[" module "] "); fprintf(file, fmt, ## __VA_ARGS__); fprintf(file, "\n"); } while(0)
#define info(module, fmt, ...) finfo(stdout, module, fmt, ## __VA_ARGS__)
#define fdebug(file, fmt, expr) ({ typeof(expr) result = expr; finfo(file, ANSI(ANSI_BOLD, ANSI_GREEN_FG) "DEBUG" ANSI_RESET_SEQUENCE, "%s", to_str_writer(stream, fprintf(stream, "%s = ", #expr); fprintf(stream, fmt, result))); result; })
#define debug(fmt, expr) fdebug(stdout, fmt, expr)

#ifdef __x86_64__
    #define __TARGET_BYTES__ 8
    typedef signed long long int isize;
typedef unsigned long long int usize;
#else
    #define __TARGET_BYTES__ 4
    typedef signed long int isize;
typedef unsigned long int usize;
#endif
#define __TARGET_BITS__ (__TARGET_BYTES__ * 8)

typedef signed char i8;
typedef signed short int i16;
typedef signed long int i32;
typedef signed long long int i64;

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned long int u32;
typedef unsigned long long int u64;

typedef float f32;
typedef double f64;

typedef char* str;

// MANUAL
#ifdef __LITTLE_ENDIAN__
    #define __ENDIANESS__ "little"
#else
    #define __ENDIUANESS__ "big"
#endif

#endif