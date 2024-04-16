#pragma once

#define __WIN32__
#define __x86_64__
#define __LITTLE_ENDIAN__

#ifdef __WIN32__
#define __PATH_SEP__ '\\'
#else
#define __PATH_SEP__ '/'
#endif

#define __FILENAME__ (strrchr(__FILE__, __PATH_SEP__) ? strrchr(__FILE__, __PATH_SEP__) + 1 : __FILE__)

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

typedef u8 bool;
#define true 1
#define false 0

// MANUAL
#ifdef __LITTLE_ENDIAN__
#define __ENDIANESS__ "little"
#else
#define __ENDIUANESS__ "big"
#endif