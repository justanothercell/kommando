#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void run_program(Program program) {
    uint8_t* memory = program.memory;
    uint64_t ip = program.ip;
    uint64_t sp = program.sp;
    uint8_t* base = memory;
    while (1) {
        uint64_t instr = READ_NEXT(ip);
        //printf("%lx %lu\n", ip, instr);
        switch (instr) {
            case OP_NOOP:
                break;
            case OP_EXIT:
                return;
            case OP_LITERAL:
                PUSH_STACK(sp, READ_NEXT(ip));
                break;
            case OP_LOAD8:
                PUSH_STACK(sp, (uint8_t)READ_WORD(READ_NEXT(ip)));
                break;
            case OP_LOAD64:
                PUSH_STACK(sp, READ_WORD(READ_NEXT(ip)));
                break;
            case OP_STORE8:
                WRITE_BYTE(READ_NEXT(ip), POP_STACK(sp));
                break;
            case OP_STORE64:
                WRITE_WORD(READ_NEXT(ip), POP_STACK(sp));
                break;
            case OP_A_LOAD64:
                WRITE_WORD(READ_NEXT(ip), *(uint8_t*)(uintptr_t)READ_NEXT(ip));
                break;
            case OP_A_STORE64:
                *(uint64_t*)READ_NEXT(ip) = POP_STACK(sp);
                break;
            case OP_S_LOAD8:
                PUSH_STACK(sp, (uint8_t)READ_WORD(POP_STACK(ip)));
                break;
            case OP_S_LOAD64:
                PUSH_STACK(sp, READ_WORD(POP_STACK(sp)));
                break;
            case OP_S_STORE8:
                WRITE_BYTE(POP_STACK(ip), POP_STACK(sp));
                break;
            case OP_S_STORE64:
                WRITE_WORD(POP_STACK(ip), POP_STACK(sp));
                break;
            case OP_PUSH_IP:
                PUSH_STACK(sp, (uint64_t)ip);
                break;
            case OP_PUSH_SP:
                PUSH_STACK(sp, (uint64_t)sp);
                break;
            case OP_SET_IP:
                ip = POP_STACK(sp);
                break;
            case OP_SET_SP:
                sp = POP_STACK(sp);
                break;
            case OP_JSR:
                PUSH_STACK(sp, (uint64_t)ip);
                ip = READ_NEXT(ip);
                break;
            case OP_RET: {
                uint64_t r = POP_STACK(sp);
                ip = POP_STACK(sp);
                PUSH_STACK(sp, r);
            } break;
            case OP_JUMP:
                ip += READ_NEXT(sp);
                break;
            case OP_LONG_JUMP:
                ip = READ_NEXT(sp);
                break;
            case OP_TO_C_ADDR:
                PUSH_STACK(sp, (uint64_t)base + POP_STACK(sp));
                break;
            case OP_JZ:
                if (POP_STACK(sp) == 0) ip += READ_NEXT(sp);
                break;
            case OP_JP:
                if ((int64_t)POP_STACK(sp) > 0) ip += READ_NEXT(sp);
                break;
            case OP_DUP: {
                uint64_t v = POP_STACK(sp);
                PUSH_STACK(sp, v);
                PUSH_STACK(sp, v);
            } break;
            case OP_SWAP:  {
                uint64_t v1 = POP_STACK(sp);
                uint64_t v2 = POP_STACK(sp);
                PUSH_STACK(sp, v1);
                PUSH_STACK(sp, v2);
            } break;
            case OP_UP2:  {
                uint64_t v1 = POP_STACK(sp);
                uint64_t v2 = POP_STACK(sp);
                uint64_t v3 = POP_STACK(sp);
                PUSH_STACK(sp, v2);
                PUSH_STACK(sp, v1);
                PUSH_STACK(sp, v3);
            } break;
            case OP_UP3:  {
                uint64_t v1 = POP_STACK(sp);
                uint64_t v2 = POP_STACK(sp);
                uint64_t v3 = POP_STACK(sp);
                uint64_t v4 = POP_STACK(sp);
                PUSH_STACK(sp, v3);
                PUSH_STACK(sp, v2);
                PUSH_STACK(sp, v1);
                PUSH_STACK(sp, v4);
            } break;
            case OP_DOWN2:  {
                uint64_t v1 = POP_STACK(sp);
                uint64_t v2 = POP_STACK(sp);
                uint64_t v3 = POP_STACK(sp);
                PUSH_STACK(sp, v1);
                PUSH_STACK(sp, v3);
                PUSH_STACK(sp, v2);
            } break;
            case OP_DOWN3:  {
                uint64_t v1 = POP_STACK(sp);
                uint64_t v2 = POP_STACK(sp);
                uint64_t v3 = POP_STACK(sp);
                uint64_t v4 = POP_STACK(sp);
                PUSH_STACK(sp, v1);
                PUSH_STACK(sp, v4);
                PUSH_STACK(sp, v3);
                PUSH_STACK(sp, v2);
            } break;
            case OP_POP:
                POP_STACK(sp);
                break;
            case OP_CALL_N: {
                uint64_t arg_c = POP_STACK(sp);
                uint64_t (*func)(uint64_t, ...) = (void*)POP_STACK(sp);
                uint64_t x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16;
                switch (arg_c) {
                    case 16: x16 = POP_STACK(sp);
                    case 15: x15 = POP_STACK(sp);
                    case 14: x14 = POP_STACK(sp);
                    case 13: x13 = POP_STACK(sp);
                    case 12: x12 = POP_STACK(sp);
                    case 11: x11 = POP_STACK(sp);
                    case 10: x10 = POP_STACK(sp);
                    case 9: x9 = POP_STACK(sp);
                    case 8: x8 = POP_STACK(sp);
                    case 7: x7 = POP_STACK(sp);
                    case 6: x6 = POP_STACK(sp);
                    case 5: x5 = POP_STACK(sp);
                    case 4: x4 = POP_STACK(sp);
                    case 3: x3 = POP_STACK(sp);
                    case 2: x2 = POP_STACK(sp);
                    case 1: x1 = POP_STACK(sp);
                    case 0: break;
                    default:
                        fprintf(stderr, "VM Error: OP_CALL_N with %lu args not supported.\n", arg_c); exit(1);
                } 
                uint64_t r = func(x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16);
            } break;
            case OP_ADD:
                PUSH_STACK(sp, POP_STACK(sp) + POP_STACK(sp));
                break;
            case OP_SUB:
                PUSH_STACK(sp, POP_STACK(sp) - POP_STACK(sp));
                break;
            case OP_MUL:
                PUSH_STACK(sp, POP_STACK(sp) * POP_STACK(sp));
                break;
            case OP_DIV:
                PUSH_STACK(sp, POP_STACK(sp) / POP_STACK(sp));
                break;
            case OP_MOD:
                PUSH_STACK(sp, POP_STACK(sp) % POP_STACK(sp));
                break;
            case OP_AND:
                PUSH_STACK(sp, POP_STACK(sp) & POP_STACK(sp));
                break;
            case OP_OR:
                PUSH_STACK(sp, POP_STACK(sp) | POP_STACK(sp));
                break;
            case OP_XOR:
                PUSH_STACK(sp, POP_STACK(sp) ^ POP_STACK(sp));
                break;
            case OP_NOT:
                PUSH_STACK(sp, ~POP_STACK(sp));
                break;
            case OP_NEG:
                PUSH_STACK(sp, -POP_STACK(sp));
                break;
            default:
                fprintf(stderr, "VM Error: Invalid OP %lu.\n", instr); exit(1);
        }
    }
}

Program program_from_file(FILE* file) {
    Program program;
    fread(&program.size, sizeof(program.size), 1, file);
    fread(&program.ip, sizeof(program.ip), 1, file);
    fread(&program.sp, sizeof(program.sp), 1, file);
    program.memory = malloc(program.size);
    fread(program.memory, sizeof(uint8_t), program.size, file);
    return program;
}

void program_to_file(FILE* file, Program program) {
    fwrite(&program.size, sizeof(program.size), 1, file);
    fwrite(&program.ip, sizeof(program.size), 1, file);
    fwrite(&program.sp, sizeof(program.size), 1, file);
    fwrite(program.memory, sizeof(uint8_t), program.size, file);
}