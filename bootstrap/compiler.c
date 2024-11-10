#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "ast.h"
#include "lib.h"
#include "module.h"
#include "parser.h"
#include "resolver.h"
#include "token.h"
#include "transpiler_c.h"

CompilerOptions build_args(StrList* args) {
    if (args->length == 1) {
        print_help: {}
        printf("%s <infile.kdo> <out> <options>\n", args->elements[0]);
        printf("Options:\n");
        printf("    --run     -r - run the compiled binary\n");
        printf("    --compile -c - compile generated c to executable\n");
        printf("    --raw     -w - do not create wrapper code\n");
        printf("    --cc=<c_compiler_path>\n");
        printf("    --dir=<output_directory>\n");
        printf("    ::lib::path=<path/to/lib_file.kdo> (multiple possible)\n");
        quit(0);
    }

    CompilerOptions options;
    options.source = NULL;
    options.outname = NULL;
    options.out_dir = NULL;
    options.c_compiler = NULL;
    options.raw = false;
    options.compile = false;
    options.run = false;
    options.modules = map_new();

    for (usize i = 1;i < args->length;i++) {
        str arg = args->elements[i];
        if (strlen(arg) == 0) continue;
        if (str_startswith(arg, "--")) {
            arg += 2;
            if (str_eq(arg, "run")) {
                options.run = true;
            } else if (str_eq(arg, "compile")) {
                options.compile = true;
            } else if (str_eq(arg, "raw")) {
                options.raw = true;
            } else if (str_startswith(arg, "dir=")){
                arg += 4;
                if (options.out_dir != NULL) panic("dir already supplied as `%s` but was supplied again: `--dir=%s`", options.out_dir, arg);
                options.out_dir = arg;
            } else if (str_startswith(arg, "cc=")){
                arg += 3;
                if (options.c_compiler != NULL) panic("c compiler already supplied as `%s` but was supplied again: `--cc=%s`", options.c_compiler, arg);
                options.c_compiler = arg;
            } else {
                log(ANSI(ANSI_RED_FG, ANSI_BOLD) "Error: " ANSI_RESET_SEQUENCE "Invalid flag `--%s`", arg);
                goto print_help;
            } 
        } else if (str_startswith(arg, "-")) {
            arg += 1;
            usize flags = strlen(arg);
            for(usize j = 0;j < flags;j++) {
                if (arg[j] == 'r') {
                    options.run = true;
                } else if (arg[j] == 'c') {
                    options.compile = true;
                } else if (arg[j] == 'w') {
                    options.raw = true;
                } else {
                    log(ANSI(ANSI_RED_FG, ANSI_BOLD) "Error: " ANSI_RESET_SEQUENCE "Invalid flag `%c` in `-%s`", arg[j], arg);
                    goto print_help;
                }
            } 
        } else if (str_startswith(arg, "::")) {
            for (usize i = 0;i < strlen(arg);i++) {
                if (arg[i] == '=') {
                    str mod = to_str_writer(stream, fprintf(stream, "%.*s", (int)i, arg));
                    str file = arg + i + 1;
                    map_put(options.modules, mod, file);
                }
            }
        } else {
            if (options.source == NULL) {
                options.source = arg;
            } else if (options.outname == NULL) {
                options.outname = arg;                
            } else {
                log(ANSI(ANSI_RED_FG, ANSI_BOLD) "Error: " ANSI_RESET_SEQUENCE "Invalid command line argument `%s`", arg);
                goto print_help;
            }
        }
    }

    if (options.run && !options.compile) panic("Cannot run without compiling (--compile|-c flag missing)");

    if (options.source == NULL) panic("No source file set");
    if (options.outname == NULL) panic("No out name set");
    if (options.out_dir == NULL) options.out_dir = "./build";
    if (options.c_compiler == NULL) options.c_compiler = "gcc";
    return options;
}

void compile(CompilerOptions options) {
    Program* program = gc_malloc(sizeof(Program));
    program->packages = map_new();

    TokenStream* stream = tokenstream_new(options.source, read_file_to_string(options.source));
    Module* main = parse_module_contents(stream, gen_path("::main"));
    ModuleItem* main_func_item = map_get(main->items, "main");
    if (main_func_item == NULL || main_func_item->type != MIT_FUNCTION) panic("no main function found");

    Module* intrinsics = gen_intrinsics();

    program->main_module = main;

    insert_module(program, main);
    insert_module(program, intrinsics);

    map_foreach(options.modules, lambda(void, str modname, str file, {
        TokenStream* s = tokenstream_new(file, read_file_to_string(file));
        Module* mod = parse_module_contents(s, gen_path(modname));
        insert_module(program, mod);
    }));

    resolve(program);

    str code_file_name = to_str_writer(stream, fprintf(stream, "%s.c", options.outname));
    str header_file_name = to_str_writer(stream, fprintf(stream, "%s.h", options.outname));
    FILE* code_file = fopen(code_file_name, "w");
    FILE* header_file = fopen(header_file_name, "w");
    transpile_to_c(options, header_file, code_file, header_file_name, program);
    fclose(header_file);
    fclose(code_file);

    if (options.compile) {
        info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "COMPILE_C" ANSI_RESET_SEQUENCE, "Compilign generated c code... ");
        str command = to_str_writer(stream, fprintf(stream, "%s -ggdb -Wall -Wswitch-enum -lm %s -o %s", options.c_compiler, code_file_name, options.outname));
        i32 r = system(command);
        if (r != 0) panic("%s failed with error code %lu", options.c_compiler, r);
        info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "COMPILE_C" ANSI_RESET_SEQUENCE, "Done!");
    }
}