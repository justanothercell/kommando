#include "../lib.h"
#include "defines.h"

#include <stdbool.h>
#include <string.h>

void* gc_malloc(usize size) {
    return malloc(size);
}

void gc_enable() { }

void gc_disable() { }

void* gc_realloc(void* ptr, usize size) {
    return realloc(ptr, size);
}

void gc_free(void *ptr) {
    free(ptr);
}

void gc_begin() { }

void gc_collect() { }

void gc_end() { }