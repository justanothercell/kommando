#include "exit.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

FILE* MEMTRACE = NULL;

void opentrace() {
    if (MEMTRACE == NULL) MEMTRACE = fopen("MEMTRACE.txt", "w");
}

uint8_t checksum32(uint8_t* data){
    uint8_t sum = 0;
    for (int i = 0;i < 3; i++){
        sum += data[i];
    }
    return ~sum + 1;
}

bool check_checksum32(uint8_t* data) {
    uint8_t sum = 0;
    for (int i = 0; i < 4; i++){
        sum += data[i];
    }
    return sum == 0;
}

void set_check(void* ptr, size_t size) {
    uint8_t* size_m = (uint8_t*)ptr;
    uint8_t* check_l = ((uint8_t*)ptr)+4;
    uint8_t* check_r = ((uint8_t*)ptr)+8+size;
    *((uint32_t*)size_m) = (uint32_t)size;
    *((uint32_t*)check_l) = rand();
    *(check_l+3) = checksum32(check_l); 
    *((uint32_t*)check_r) = rand();
    *(check_r+3) = checksum32(check_r); 
}

void check_check(void* ptr) {
    uint8_t* size_m = (uint8_t*)ptr;
    uint8_t* check_l = ((uint8_t*)ptr)+4;
    uint8_t* check_r = ((uint8_t*)ptr)+8+(*(uint32_t*)size_m);
    if (!check_checksum32(check_l)) {
        spanic("block checksum (left) not valid for %p: Wrote over preceeding bytes.\n", (char*)ptr + 8);
    }
    if (!check_checksum32(check_r)) {
        spanic("Block checksum (right) not valid for %p: Wrote over succeeding bytes.\n", (char*)ptr + 8);
    }
}

void* debug_malloc(size_t size, char* file, int line) {
    opentrace();
    void* ptr = malloc(size + 12);
    fprintf(MEMTRACE, "MALLOC %p %lu @ %s:%d\n", (char*)ptr + 8, size, file, line);
    fflush(MEMTRACE);
    set_check(ptr, size);
    return (char*)ptr + 8;
}

void* debug_realloc(void* ptr, size_t size, char* file, int line) {
    if (ptr != NULL) {
        ptr = (char*)ptr - 8;
        check_check(ptr);
    }
    opentrace();
    usize p = (usize)ptr;
    void* new_ptr = realloc(ptr, size + 12);
    if (ptr == NULL) fprintf(MEMTRACE, "REALLOC %p %p %lu @ %s:%d\n", (char*)p, (char*)new_ptr + 8, size, file, line);
    else fprintf(MEMTRACE, "REALLOC %p %p %lu @ %s:%d\n", (char*)p + 8, (char*)new_ptr + 8, size, file, line);
    fflush(MEMTRACE);
    set_check(new_ptr, size);
    return (char*)new_ptr + 8;
}

void debug_free(void* ptr, char* file, int line) {
    if (ptr != NULL) {
        ptr = (char*)ptr - 8;
        check_check(ptr);
    }
    opentrace();
    if (ptr == NULL) fprintf(MEMTRACE, "FREE %p @ %s:%d\n", ptr, file, line);
    else fprintf(MEMTRACE, "FREE %p @ %s:%d\n", (char*)ptr + 8, file, line);
    free(ptr);
    fflush(MEMTRACE);
}

void __raw_free(void* ptr) {
    free(ptr);
}