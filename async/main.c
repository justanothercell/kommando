#include <stdlib.h>
#include <stdio.h>
#include <sys/ucontext.h>
#include <ucontext.h>

typedef struct async_ctx {
    ucontext_t ctx;
    ucontext_t ret;
    int status;
    volatile void* ret_val;
} async_ctx;

#define NUMARGS(...) sizeof((char[]){__VA_ARGS__})

#define __async__ async_ctx* __async_ctx__
#define yield swapcontext(&__async_ctx__->ctx, &__async_ctx__->ret)
#define async_ret(value) do { \
    __async_ctx__->status = 1; \
    *(typeof(value)*)__async_ctx__->ret_val = value; \
    yield; \
} while (0)
#define await(__f__, ...) ({ \
    run_blocking(__f__, ##__VA_ARGS__); \
})
#define run_blocking(__f__, ...) ({ \
    async_ctx __async_ctx__; \
    getcontext(&__async_ctx__.ctx); \
    char stack[16384]; /*8192*/ \
    __async_ctx__.ctx.uc_stack.ss_size = sizeof(stack); \
    __async_ctx__.ctx.uc_stack.ss_sp = stack; \
    __async_ctx__.ctx.uc_link = &__async_ctx__.ret; \
    typeof(__f__(&__async_ctx__, ##__VA_ARGS__)) __r__ = {};\
    __async_ctx__.ret_val = &__r__; \
    __async_ctx__.status = 0; \
    makecontext(&__async_ctx__.ctx, (void (*)(void))__f__, NUMARGS(0, ##__VA_ARGS__), &__async_ctx__, ##__VA_ARGS__); \
    while (__async_ctx__.status == 0) { \
        swapcontext(&__async_ctx__.ret, &__async_ctx__.ctx); \
    } \
    __r__; \
})

int doubler(__async__, int val)  {
    yield;
    printf("computing...\n");
    yield;
    printf("computing...\n");
    async_ret(val * 2);
}

int f(__async__) {
    printf("hello from f, awaiting the doubler...\n");
    int r = await(doubler, 21);
    printf("doubler done: %d!\n", r);
    async_ret(42);
}

int main() {
    printf("hello from main\n");
    int r = run_blocking(f);
    printf("f returned with value %d\n", r);
}