#ifndef DEFINES_H
#define DEFINES_H

#ifdef __x86_64__
    #define __TARGET_BITS__ 64
    typedef signed long long int isize;
typedef unsigned long long int usize;
#else
    #define __TARGET_BITS__ 32
    typedef signed long int isize;
typedef unsigned long int usize;
#endif

typedef signed char i8;
typedef signed short int i16;
typedef signed long int i32;
typedef signed long long int i64;

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned long int u32;
typedef unsigned long long int u64;

typedef float f32;
typedef long f64;

typedef char* str;
typedef void* any;

// MANUAL
#ifdef __LITTLE_ENDIAN__
    #define __ENDIANESS__ "little"
#else
    #define __ENDIUANESS__ "big"
#endif

#endif