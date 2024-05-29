#ifndef LIB_DEFINES_H
#define LIB_DEFINES_H

#include <stdbool.h>
#include <stdio.h>

#define lambda(ret_type, _body) ({ ret_type f1234567890 _body; f1234567890; })

// #define __TRACE__

#ifdef __TRACE__
    static int trace_indent = 0;
    
    #define TRACE(expr) ({ \
        for (int i = 0;i < trace_indent;i++) { printf("| "); } \
        printf("*>call %s from %s %s:%d\n", #expr, __func__, __FILENAME__, __LINE__); \
        trace_indent++; \
        typeof(expr) out = expr; \
        trace_indent--; \
        for (int i = 0;i < trace_indent;i++) { printf("| "); } \
        printf("*<exit %s from %s %s:%d\n", #expr, __func__, __FILENAME__, __LINE__); \
        out; \
    })
    #define TRACEV(expr) ({ \
        for (int i = 0;i < trace_indent;i++) { printf("| "); } \
        printf("*>call %s from %s %s:%d\n", #expr, __func__, __FILENAME__, __LINE__); \
        trace_indent++; \
        expr; \
        trace_indent--; \
        for (int i = 0;i < trace_indent;i++) { printf("| "); } \
        printf("*<exit %s from %s %s:%d\n", #expr, __func__, __FILENAME__, __LINE__); \
    })
#else
    #define TRACE(expr) expr
    #define TRACEV(expr) expr
#endif

#ifdef __WIN32__
    #define __PATH_SEP__ '\\'
#else
    #define __PATH_SEP__ '/'
#endif

#define __FILENAME__ (strrchr(__FILE__, __PATH_SEP__) ? strrchr(__FILE__, __PATH_SEP__) + 1 : __FILE__)

#define FORMATTER(expr) (_Generic((expr), \
                i8: "%c", \
                u8: "%c", \
                i16: "%d", \
                u16: "%u", \
                int: "%i", \
                i32: "%ld", \
                u32: "%lu", \
                i64: "%lld", \
                u64: "%llu", \
                str: "%s", \
                f32: "%f", \
                f64: "%lf", \
                default: "%p" \
            ))

#define fprint_prefix_raw(file, srcfile, line) fprintf(file, "[%s:%i] ", srcfile, line)
#define fprint_prefix(file) fprint_prefix_raw(file, __FILENAME__, __LINE__)
#define print_prefix() fprint_prefix(stdout)
#define flog(file, fmt, ...) do { fprint_prefix(file); fprintf(file, fmt, ## __VA_ARGS__); fprintf(file, "\n"); } while(0)
#define log(fmt, ...) flog(stdout, fmt, ## __VA_ARGS__)
#define finfo(file, module, fmt, ...) do { fprint_prefix(file); fprintf(file, "[" module "] "); printf(fmt, ## __VA_ARGS__); fprintf(file, "\n"); } while(0)
#define info(module, fmt, ...) finfo(stdout, module, fmt, ## __VA_ARGS__)
#define fdebug(file, expr) ({ typeof(expr) result = expr; fprint_prefix(file); fprintf(file, FORMATTER(result), result); fprintf(file, "\n"); result; })
#define debug(...) fdebug(stdout, __VA_ARGS__)

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
typedef void* any;

// MANUAL
#ifdef __LITTLE_ENDIAN__
    #define __ENDIANESS__ "little"
#else
    #define __ENDIUANESS__ "big"
#endif

#endif