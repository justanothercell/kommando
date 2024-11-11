#ifndef LIB_DEFINES_H
#define LIB_DEFINES_H

#include <stdbool.h>
#include <stdio.h>

#include "terminal.h"

#define UNUSED(x) (void)x

#define IDENTITY_MACRO(x) x
#define STRINGIFY(x) #x

#define VALUE_IF_ELSE_TEST(x, y) y
#define VALUE_IF_ELSE_TEST0(x, y) y
#define VALUE_IF_ELSE_TEST1(x, y) x
#define VALUE_IF_ELSE(COND, x, y) VALUE_IF_ELSE_TEST ## COND (x, y)

#define VALUE_IF_TEST(...)
#define VALUE_IF_TEST0(...)
#define VALUE_IF_TEST1(...) __VA_ARGS__
#define VALUE_IF(COND, ...) VALUE_IF_TEST ## COND (__VA_ARGS__)

#define VALUE_UNLESS_TEST(...) __VA_ARGS__
#define VALUE_UNLESS_TEST0(...) __VA_ARGS__
#define VALUE_UNLESS_TEST1(...)
#define VALUE_UNLESS(COND, ...) VALUE_UNLESS_TEST ## COND (__VA_ARGS__)

#ifndef __INTELLISENSE__
#define _lambda0(x, ...) VALUE_IF(__VA_OPT__(1), , x)
#define _lambda1(x, ...) VALUE_IF(__VA_OPT__(1), , x _lambda0(__VA_ARGS__))
#define _lambda2(x, ...) VALUE_IF(__VA_OPT__(1), , x _lambda1(__VA_ARGS__))
#define _lambda3(x, ...) VALUE_IF(__VA_OPT__(1), , x _lambda2(__VA_ARGS__))
#define _lambda4(x, ...) VALUE_IF(__VA_OPT__(1), , x _lambda3(__VA_ARGS__))
#define _lambda5(x, ...) VALUE_IF(__VA_OPT__(1), , x _lambda4(__VA_ARGS__))
#define _lambda6(x, ...) VALUE_IF(__VA_OPT__(1), , x _lambda5(__VA_ARGS__))
#define _lambda7(x, ...) VALUE_IF(__VA_OPT__(1), , x _lambda6(__VA_ARGS__))
#define _lambda8(x, ...) VALUE_IF(__VA_OPT__(1), x _lambda7(__VA_ARGS__))
#define _body0(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x, x)
#define _body1(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body0(__VA_ARGS__), x)
#define _body2(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body1(__VA_ARGS__), x)
#define _body3(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body2(__VA_ARGS__), x)
#define _body4(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body3(__VA_ARGS__), x)
#define _body5(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body4(__VA_ARGS__), x)
#define _body6(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body5(__VA_ARGS__), x)
#define _body7(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body6(__VA_ARGS__), x)
#define _body8(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), _body7(__VA_ARGS__), x)
#define lambda(ret_type, ...) ({ ret_type f1234567890 ( _lambda8(__VA_ARGS__) ) _body8(__VA_ARGS__); f1234567890; })

#define lambda_return return
#else
// dummy def since intellij/clang doesnt know inner functions
#define _body(body) body;
#define _lambda1(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _body(__VA_ARGS__), x;)
#define _lambda2(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda1(__VA_ARGS__), x;)
#define _lambda2(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda1(__VA_ARGS__), x;)
#define _lambda3(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda2(__VA_ARGS__), x;)
#define _lambda4(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda3(__VA_ARGS__), x;)
#define _lambda5(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda4(__VA_ARGS__), x;)
#define _lambda6(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda5(__VA_ARGS__), x;)
#define _lambda7(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda6(__VA_ARGS__), x;)
#define _lambda8(x, ...) VALUE_IF_ELSE(__VA_OPT__(1), x = {}; _lambda7(__VA_ARGS__), x;)
#define lambda(ret_type, ...) ({ void* f1234567890(); _lambda8(__VA_ARGS__) f1234567890; })

#define lambda_return
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