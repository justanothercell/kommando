#include "../lib.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>

static void* STACK_BASE = NULL; 

static u8 GC_FLAG_UNTAGGED = 0b0;
static u8 GC_MASK_TAGGED = 0b1;

typedef struct GcAllocation {
    void* ptr;
    usize size;
    u8 flag;
} GcAllocation;
LIST(GcAllocList, GcAllocation);

static GcAllocList ALLOCATIONS = list_new();
static usize ENTROPY_COUNTER = 0;
#define ENTROPY_LIMIT 1024

GcAllocation new_alocation(void* ptr, usize size) {
    return (GcAllocation) { ptr, size, 0 };
}

void* gc_malloc(usize size) {
    if (STACK_BASE == NULL) panic("GC not initialized");
    void* ptr = malloc(size);
    list_append(ALLOCATIONS, new_alocation(ptr, size));
    if (ENTROPY_COUNTER++ > ENTROPY_LIMIT) gc_collect();
    return ptr;
}

void* gc_realloc(void* ptr, usize size) {
    if (STACK_BASE == NULL) panic("GC not initialized");
    void* new_ptr = realloc(ptr, size);
    list_find_map(ALLOCATIONS, item, item.ptr == ptr, lambda(void, (GcAllocation* a) { a->ptr = new_ptr; a->size = size; }));
    if (ENTROPY_COUNTER++ > ENTROPY_LIMIT) gc_collect();
    return new_ptr;
}

void gc_free(void *ptr) {
    if (STACK_BASE == NULL) panic("GC not initialized");
    free(ptr);
    list_find_swap_remove(ALLOCATIONS, item, item.ptr == ptr);
    if (ENTROPY_COUNTER++ > ENTROPY_LIMIT) gc_collect();
}

void gc_begin() {
    if (STACK_BASE != NULL) panic("GC already initialized");
    STACK_BASE = __builtin_frame_address(1);
}

void gc_end() {
    if (STACK_BASE == NULL) panic("GC not initialized");
    gc_collect();
    STACK_BASE = NULL;
}

void gc_flag_allocs(void* ptr, usize size) {
    for(void** p = ptr; (usize)p <= (usize)ptr + size - sizeof(void*); p++) {
        list_find_map(ALLOCATIONS, item, item.ptr == *p, lambda(GcAllocation, (GcAllocation* alloc) {
            if (alloc->flag & GC_MASK_TAGGED == 0) {
                alloc->flag |= GC_MASK_TAGGED; 
                gc_flag_allocs(item.ptr, item.size);
            }
        }));
    }
}

void gc_flag_stack() {
    void* stack_top = __builtin_frame_address(0);
    // stack grows from top to bottom
    gc_flag_allocs(stack_top, (usize)STACK_BASE - (usize)stack_top);
}

void gc_cleanup() {
    list_map_retain(ALLOCATIONS, lambda(bool, (GcAllocation* alloc) {
        if (alloc->flag & GC_MASK_TAGGED != 0) {
            alloc->flag &= ~GC_MASK_TAGGED;
            return true;
        }
        return false;
    }));
}

void gc_collect() {
    if (STACK_BASE == NULL) panic("GC not initialized");

    ENTROPY_COUNTER = 0;

    if (ALLOCATIONS.length == 0) {
        info("GC", "No values to collect");
        return;
    }

    usize len_before = ALLOCATIONS.length;

    // mock to put registers onto the stack
    jmp_buf ctx;
    setjmp(ctx);
    // traverse the stack and values recursively
    gc_flag_stack();
    // cleanup unflagged values and reset flags
    gc_cleanup();
    // make sure ctx is kept
    volatile usize _ctx = (usize)&ctx;

    usize len_after = ALLOCATIONS.length;
    info("GC", "Collected %llu/%llu allocations (%2.3f%%)", len_before - len_after, len_before, (f32)(len_before - len_after) * 100.0 / (float) len_before);
}
