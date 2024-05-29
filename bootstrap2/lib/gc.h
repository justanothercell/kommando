#ifndef LIB_GC_H
#define LIB_GC_H

#include "defines.h"

void* gc_malloc(usize size);
void* gc_realloc(void* ptr, usize size);
void gc_free(void* ptr);

void gc_begin();

void gc_end();
void gc_collect();

#endif