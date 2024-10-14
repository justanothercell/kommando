#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdio.h>

#define WORD_SIZE 8
#define write_arg(ptr, arg) do { *(uint64_t*)(ptr) = (uint64_t)(arg); (ptr) += WORD_SIZE;} while(0)
#define align_ptr(ptr) (ptr) += (WORD_SIZE - (uintptr_t)(ptr) % WORD_SIZE) % WORD_SIZE
#define write_data(ptr, data_ptr, length) do { uint64_t len1234567890 = (length); memcpy((ptr), (data_ptr), len1234567890); (ptr) += len1234567890; } while(0)
#define write_data_aligned(ptr, data_ptr, length) write_data(ptr, data_ptr, length); align_ptr(ptr);

#define READ_NEXT(ip) ({ uint64_t v = *(uint64_t*)(base + ip); ip += WORD_SIZE; v; })
#define READ_WORD(addr) ({ uint64_t v = *(uint64_t*)(base + addr); v; })
#define WRITE_WORD(addr, value) ({ *(uint64_t*)(base + addr) = (value); })
#define WRITE_BYTE(addr, value) ({ *(base + addr) = (value); })
#define PUSH_STACK(sp, word) ({ uint64_t v1234567890 = (word); *(uint64_t*)(base + sp + WORD_SIZE) = v1234567890; sp += WORD_SIZE; })
#define POP_STACK(sp) ({ uint64_t v = *(uint64_t*)(base + sp); sp -= WORD_SIZE; v; })

typedef enum Op {
    OP_NOOP,
    OP_EXIT,
    OP_LITERAL,
    OP_LOAD8,
    OP_LOAD64,
    OP_STORE8,
    OP_STORE64,
    OP_A_LOAD64,
    OP_A_STORE64,
    OP_S_LOAD8,
    OP_S_LOAD64,
    OP_S_STORE8,
    OP_S_STORE64,

    OP_PUSH_IP,
    OP_PUSH_SP,
    OP_PUSH_RP,
    OP_SET_IP,
    OP_SET_SP,
    OP_SET_RP,

    OP_JSR,
    OP_RET,
    OP_JUMP,
    OP_JZ,
    OP_JP,
    OP_LONG_JUMP,
    OP_TO_C_ADDR,

    OP_DUP,
    OP_SWAP,
    OP_UP2,
    OP_UP3,
    OP_DOWN2,
    OP_DOWN3,
    OP_POP,

    OP_CALL_N,

    OP_ADD,
    OP_MUL,
    OP_SUB,
    OP_DIV,
    OP_MOD,
    OP_AND,
    OP_OR,
    OP_XOR,

    OP_NOT,
    OP_NEG
} Code;

typedef struct Program {
    uint8_t* memory;
    uint64_t ip;
    uint64_t sp;
    uint64_t size;
} Program;

void run_program(Program Program);

Program program_from_file(FILE* file);
void program_to_file(FILE* file, Program program);

#endif