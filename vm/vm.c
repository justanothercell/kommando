#include "vm.h"
#include <stdlib.h>

void run(u8* memory) {
    u8* base = memory;
    u8* pc = base;
    u8* sp;

    while (1) {
        u8 instr = *pc++;
        switch (instr) {
            default:
                exit(1);
        } 
    }
}

int next(u8* pc) {
    *pc += 4;
    return *pc - 4;
}