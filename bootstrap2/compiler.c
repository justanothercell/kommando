#include "compiler.h"
#include "ast.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
#include "module.h"
#include "parser.h"
#include "resolver.h"
#include "token.h"

CompilerOptions build_args(StrList* args) {
    if (args->length == 1) {
        printf("%s <infile.kdo> <out> <options>\n", args->elements[0]);
        printf("Options:\n");
        printf("    --run     -r - run the compiled binary\n");
        printf("    --compile -c - compile generated c to executable\n");
        printf("    --raw     -w - do not create wrapper code\n");
        printf("    --dir=<output_directory>\n");
        quit(0);
    }

    CompilerOptions options;
    options.source = NULL;
    options.outname = NULL;
    options.out_dir = NULL;
    options.raw = false;
    options.compile = false;
    options.run = false;

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
            } else {
                panic("Invalid flag `--%s`", arg);
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
                    panic("Invalid flag `%c` in `-%s`", arg[j], arg);
                }
            } 
        } else {
            if (options.source == NULL) {
                options.source = arg;
            } else if (options.outname == NULL) {
                options.outname = arg;                
            } else {
                panic("Invalid command line argument `%s`", arg);
            }
        }
    }

    if (options.run && !options.compile) panic("Cannot run without compiling (--compile|-c flag missing)");

    if (options.source == NULL) panic("No source file set");
    if (options.outname == NULL) panic("No out name set");
    if (options.out_dir == NULL) options.out_dir = "./build";
    return options;
}

void compile(str file, str source) {
    Program* program = gc_malloc(sizeof(Program));
    program->modules = map_new();

    TokenStream* stream = tokenstream_new(file, source);
    Module* main = parse_module_contents(stream, path_new(true, list_new(IdentList)));
    Module* std = gen_std();

    program->main_module = main;
    map_put(program->modules, to_str_writer(stream, fprint_path(stream, main->path)), main);
    map_put(program->modules, to_str_writer(stream, fprint_path(stream, std->path)), std);

    resolve(program);
}