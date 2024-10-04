#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    uint64_t nullvalue = 0;

    uint64_t memory_size = 0x2000;
    uint8_t* memory = malloc(memory_size);
    uint64_t code_base = 0;
    uint8_t* code = memory + code_base;
    uint8_t* data_base = memory + 0x0A00;
    uint8_t* data = data_base;
    uint64_t stack = 0x1000;

    char* message = "The sum of %lu and %lu is %lu\n";
    uint64_t message_ptr = data - memory;
    write_data_aligned(data, message, strlen(message) + 1);
    uint64_t printf_ptr = data - memory;
    write_data_aligned(data, &nullvalue, WORD_SIZE);

    uint64_t adder = code - memory;
    write_arg(code, OP_DOWN2);
    write_arg(code, OP_ADD);
    write_arg(code, OP_RET);

    uint64_t entry = code - memory;
    write_arg(code, OP_LITERAL);
    write_arg(code, message_ptr);
    write_arg(code, OP_TO_C_ADDR);
    write_arg(code, OP_LITERAL);
    write_arg(code, 69);
    write_arg(code, OP_LITERAL);
    write_arg(code, 42);
        write_arg(code, OP_LITERAL);
        write_arg(code, 69);
        write_arg(code, OP_LITERAL);
        write_arg(code, 42);
        write_arg(code, OP_JSR);
        write_arg(code, adder);
    write_arg(code, OP_LITERAL);
    write_arg(code, printf_ptr);
    write_arg(code, OP_S_LOAD64);
    write_arg(code, OP_LITERAL);
    write_arg(code, 4);
    write_arg(code, OP_CALL_N);
    write_arg(code, OP_EXIT);
    
    Program program = { memory, entry, stack, memory_size };
    FILE* out = fopen("program", "w");
    program_to_file(out, program);
    fclose(out);

    FILE* in = fopen("program", "r");
    Program from_file = program_from_file(in);
    fclose(in);
    
    *(uint64_t*)(from_file.memory+printf_ptr) = (uint64_t)(void*)printf;

    run_program(from_file);
}