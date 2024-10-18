#include "../lib.h"

#include <setjmp.h>
#include <stdbool.h>
#include <string.h>

static void* STACK_BASE = NULL; 

#define GC_FLAG_UNTAGGED 0b0
#define GC_MASK_TAGGED 0b1

typedef struct GcAllocation {
    void* ptr;
    usize size;
    u8 flag;
} GcAllocation;
LIST(GcAllocList, GcAllocation);

static GcAllocList ALLOCATIONS;
static usize ENTROPY_COUNTER = 0;
#define ENTROPY_LIMIT 1024

GcAllocation new_allocation(void* ptr, usize size) {
    return (GcAllocation) { ptr, size, (u8)0 };
}

bool GC_ENABLED = true;

void* gc_malloc(usize size) {
    if (STACK_BASE == NULL) panic("GC not initialized");
    void* ptr = malloc(size);
    
    // manually doing this to avoid some cyclic behavior
    if (ALLOCATIONS.length == ALLOCATIONS.capacity) {
        ALLOCATIONS.capacity += 1;
        ALLOCATIONS.capacity *= 2;
        ALLOCATIONS.elements = realloc(ALLOCATIONS.elements, ALLOCATIONS.capacity * sizeof(GcAllocation));
    }
    ALLOCATIONS.elements[ALLOCATIONS.length] = new_allocation(ptr, size);
    ALLOCATIONS.length += 1;

    if (ENTROPY_COUNTER++ > ENTROPY_LIMIT && GC_ENABLED) gc_collect();
    return ptr;
}

void gc_enable() {
    GC_ENABLED = true;
    if (ENTROPY_COUNTER > ENTROPY_LIMIT) gc_collect();
}

void gc_disable() {
    GC_ENABLED = false;
}

void* gc_realloc(void* ptr, usize size) {
    if (STACK_BASE == NULL) panic("GC not initialized");
    if (ptr == NULL) return gc_malloc(size);
    void* new_ptr = realloc(ptr, size);
    if (!list_find_map(&ALLOCATIONS, item, item->ptr == ptr, lambda(void, (GcAllocation* a) { a->ptr = new_ptr; a->size = size; }))) {
        panic("Tried reallocating untracked pointer %p", ptr);
    }
    if (ENTROPY_COUNTER++ > ENTROPY_LIMIT && GC_ENABLED) gc_collect();
    return new_ptr;
}
//0x64e631379
//0x64e6313792a0
void gc_free(void *ptr) {
    if (STACK_BASE == NULL) panic("GC not initialized");
    free(ptr);
    list_find_swap_remove(&ALLOCATIONS, item, item->ptr == ptr);
    if (ENTROPY_COUNTER++ > ENTROPY_LIMIT && GC_ENABLED) gc_collect();
}

void gc_begin() {
    if (STACK_BASE != NULL) panic("GC already initialized");
    STACK_BASE = __builtin_frame_address(1);
    ALLOCATIONS = list_new(GcAllocList);
    gc_enable();
}

typedef struct GcCleanupResult {
    usize length_before;
    usize size_before;
    usize length_after;
    usize size_after;
} GcCleanupResult;


static void gc_flag_allocs(void* ptr, usize size) {
    if (ptr == NULL) return;
    for(void** p = ptr; (usize)p <= (usize)ptr + size - sizeof(void*); p++) {
        list_find_map(&ALLOCATIONS, item, item->ptr == *p, lambda(GcAllocation, (GcAllocation* alloc) {
            if ((alloc->flag & GC_MASK_TAGGED) == 0) {
                alloc->flag |= GC_MASK_TAGGED; 
                gc_flag_allocs(alloc->ptr, alloc->size);
            }
        }));
    }
}

void gc_flag_stack() {
    void* stack_top = __builtin_frame_address(0);
    // stack grows from top to bottom
    gc_flag_allocs(stack_top, (usize)STACK_BASE - (usize)stack_top);
}

usize X = 0;
GcCleanupResult         gc_cleanup() {
    usize length_before = ALLOCATIONS.length;
    usize size_before = 0;
    usize size_after = 0;
    list_map_retain(&ALLOCATIONS, lambda(bool, (GcAllocation* alloc) {
        size_before += alloc->size;
        if (alloc->flag & GC_MASK_TAGGED != 0) {
            size_after += alloc->size;
            alloc->flag &= ~GC_MASK_TAGGED;
            return true;
        }
        free(alloc->ptr);
        return false;
    }));
    return (GcCleanupResult) { length_before, size_before, ALLOCATIONS.length, size_after };
}

void gc_collect() {
    if (STACK_BASE == NULL) panic("GC not initialized");
    ENTROPY_COUNTER = 0;

    if (ALLOCATIONS.length == 0) {
        info("GC", "No values to collect");
        return;
    }

    // mock to put registers onto the stack
    jmp_buf ctx;
    memset(ctx, 0, sizeof(jmp_buf));
    setjmp(ctx);
    // traverse the stack and values recursively
    gc_flag_stack();
    // cleanup unflagged values and reset flags
    GcCleanupResult r = gc_cleanup();
    // make sure ctx is kept
    volatile usize _ctx = (usize)&ctx;

    usize delta_length = r.length_before - r.length_after;
    usize delta_size = r.size_before - r.size_after;
    info(ANSI(ANSI_BOLD,ANSI_CYAN_FG)"GC"ANSI_RESET_SEQUENCE, "Freed "ANSI(ANSI_BOLD,ANSI_CYAN_FG)"% 3llu"ANSI_RESET_SEQUENCE"/"ANSI(ANSI_BOLD,ANSI_CYAN_FG)"% 3llu"ANSI_RESET_SEQUENCE" ("ANSI(ANSI_BOLD,ANSI_CYAN_FG)"%06.3f%%"ANSI_RESET_SEQUENCE") - "ANSI(ANSI_BOLD,ANSI_CYAN_FG)"%.3fKB"ANSI_RESET_SEQUENCE"/"ANSI(ANSI_BOLD,ANSI_CYAN_FG)"%.3fKB"ANSI_RESET_SEQUENCE" (%06.3f%%) allocations, new heap size %.3fKB", 
        delta_length, r.length_before, (f32)(delta_length) * 100.0 / (f32) r.length_before,
        (f32)delta_size / 1024.0, (f32)r.size_before / 1024.0, (f32)(delta_size) * 100.0 / (f32) r.size_before, (f32)r.size_after / 1024.0
    );
}

void gc_end() {
    if (STACK_BASE == NULL) panic("GC not initialized");
    GcCleanupResult r = gc_cleanup();
    info(ANSI(ANSI_BOLD,ANSI_CYAN_FG)"GC"ANSI_RESET_SEQUENCE, "Cleaning up: Freed "ANSI(ANSI_BOLD,ANSI_CYAN_FG)"%llu"ANSI_RESET_SEQUENCE" allocations ("ANSI(ANSI_BOLD,ANSI_CYAN_FG)"%.3fKB"ANSI_RESET_SEQUENCE")", r.length_before, (f32) r.size_before / 1024.0);
    free(ALLOCATIONS.elements);
    ALLOCATIONS = list_new(GcAllocList);
    STACK_BASE = NULL;
}