#include <malloc.h>

void* __raw_malloc(size_t size) {
    return malloc(size);
}

void* __raw_realloc(void* ptr, size_t size){
    return realloc(ptr, size);
}

void __raw_free(void* ptr) {
    free(ptr);
}