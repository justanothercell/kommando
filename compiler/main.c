#pragma once

#include "tokens.c"
#include "ast.c"
#include "transpile.c"
#include "lib/defines.c"
#include <io.h>
#include <stdio.h>
#include <string.h>
#include "lib/os.c"
#include "types.c"

int main(const int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage:\n  %s <input.kdo> <output>\n", argv[0]);
        exit(0);
    }
    const char* INPUT_FILE = argv[1];
    const char* OUTNAME = argv[2];
    char* OUT_C_FILE = malloc(strlen(argv[1] + 2));
    sprintf(OUT_C_FILE, "%s.c", OUTNAME);
    char* OUT_H_FILE = malloc(strlen(argv[1] + 2));
    sprintf(OUT_H_FILE, "%s.h", OUTNAME);

    FILE *source = fopen(INPUT_FILE, "r");
    TokenStream* stream = new_tokenstream(source);
    Module* module = parse_module(stream);
    drop_tokenstream(stream);
    
    mkdir("build");
    if (chdir("build") != 0) {
        printf("`cd build` failed\n");
        exit(1);
    }

    FILE *code = fopen(OUT_C_FILE, "w");
    FILE *header = fopen(OUT_H_FILE, "w");
    Writer* writer = new_writer("test", code, header);
    transpile_module(writer, module);
    drop_writer(writer);

    printf("compiling generated c...\n");
    
#ifdef __WIN32__
    const char* BUILD_CMD_TEMPLATE = "cl %s /permissive- /o %s";
    char* command = malloc(sizeof(char) * (strlen(BUILD_CMD_TEMPLATE) + strlen(OUT_C_FILE) + strlen(OUTNAME) + 1));
    sprintf(command, BUILD_CMD_TEMPLATE, OUT_C_FILE, OUTNAME);
#else
    const char* BUILD_CMD_TEMPLATE = "gcc -c %s -o %s";
    char* command = malloc(sizeof(char) * (strlen(BUILD_CMD_TEMPLATE) + strlen(OUT_C_FILE) + strlen(OUTNAME) + 1));
    sprintf(command, BUILD_CMD_TEMPLATE, OUT_C_FILE, OUTNAME);
#endif
    if (system(command) != 0) {
        printf("Compilation failed\n");
        exit(1);
    }

    printf("Compilation finished successfully.\n");

    int return_code = system(OUTNAME);
    printf("Program exited with code %d.", return_code);
}