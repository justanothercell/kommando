#ifndef LIB_ENUM_H
#define LIB_ENUM_H

#include "defines.h"

#define FIELDS1(macro, f, ...) macro(f)
#define FIELDS2(macro, f, ...) macro(f) __VA_OPT__(, FIELDS1(macro,__VA_ARGS__))
#define FIELDS3(macro, f, ...) macro(f) __VA_OPT__(, FIELDS2(macro,__VA_ARGS__))
#define FIELDS4(macro, f, ...) macro(f) __VA_OPT__(, FIELDS3(macro,__VA_ARGS__))
#define FIELDS5(macro, f, ...) macro(f) __VA_OPT__(, FIELDS4(macro,__VA_ARGS__))
#define FIELDS6(macro, f, ...) macro(f) __VA_OPT__(, FIELDS5(macro,__VA_ARGS__))
#define FIELDS7(macro, f, ...) macro(f) __VA_OPT__(, FIELDS6(macro,__VA_ARGS__))
#define FIELDS8(macro, f, ...) macro(f) __VA_OPT__(, FIELDS7(macro,__VA_ARGS__))
#define FIELDS9(macro, f, ...) macro(f) __VA_OPT__(, FIELDS8(macro,__VA_ARGS__))
#define FIELDS10(macro, f, ...) macro(f) __VA_OPT__(, FIELDS9(macro,__VA_ARGS__))
#define FIELDS11(macro, f, ...) macro(f) __VA_OPT__(, FIELDS10(macro,__VA_ARGS__))
#define FIELDS12(macro, f, ...) macro(f) __VA_OPT__(, FIELDS11(macro,__VA_ARGS__))
#define FIELDS13(macro, f, ...) macro(f) __VA_OPT__(, FIELDS12(macro,__VA_ARGS__))
#define FIELDS14(macro, f, ...) macro(f) __VA_OPT__(, FIELDS13(macro,__VA_ARGS__))
#define FIELDS15(macro, f, ...) macro(f) __VA_OPT__(, FIELDS14(macro,__VA_ARGS__))
#define FIELDS16(macro, f, ...) macro(f) __VA_OPT__(, FIELDS15(macro,__VA_ARGS__))
#define FIELDS17(macro, f, ...) macro(f) __VA_OPT__(, FIELDS16(macro,__VA_ARGS__))
#define FIELDS18(macro, f, ...) macro(f) __VA_OPT__(, FIELDS17(macro,__VA_ARGS__))
#define FIELDS19(macro, f, ...) macro(f) __VA_OPT__(, FIELDS18(macro,__VA_ARGS__))
#define FIELDS20(macro, f, ...) macro(f) __VA_OPT__(, FIELDS19(macro,__VA_ARGS__))
#define FIELDS21(macro, f, ...) macro(f) __VA_OPT__(, FIELDS20(macro,__VA_ARGS__))
#define FIELDS22(macro, f, ...) macro(f) __VA_OPT__(, FIELDS21(macro,__VA_ARGS__))
#define FIELDS23(macro, f, ...) macro(f) __VA_OPT__(, FIELDS22(macro,__VA_ARGS__))
#define FIELDS24(macro, f, ...) macro(f) __VA_OPT__(, FIELDS23(macro,__VA_ARGS__))
#define FIELDS(macro, ...) FIELDS24(macro,__VA_ARGS__)

#define ENUM(name, ...) typedef enum name { FIELDS(IDENTITY_MACRO, __VA_ARGS__) } name; static char* name##__NAMES[] = { FIELDS(STRINGIFY, __VA_ARGS__) } /* NOLINT(writable-strings) */

#endif