#ifndef LIB_DEBUG_H
#define LIB_DEBUG_H

#include "stddef.h"

#define DEBUG_MEM
// #define DEBUG_CACHE

void* debug_malloc(size_t size, char* file, int line);
void* debug_realloc(void* ptr, size_t size, char* file, int line);
void debug_free(void* ptr, char* file, int line);
void __raw_free(void* ptr);

#define raw_free(ptr) __raw_free(ptr)

#ifdef DEBUG_MEM
    #define malloc(size) debug_malloc((size), __FILE__, __LINE__)
    #define realloc(ptr, size) debug_realloc((ptr), (size), __FILE__, __LINE__)
    #define free(ptr) debug_free((ptr), __FILE__, __LINE__)
#else
    #include <malloc.h>
#endif

#endif