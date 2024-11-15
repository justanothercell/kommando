#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "compiler.h"
#include "ast.h"
#include "lib.h"
LIB;
#include "module.h"
#include "parser.h"
#include "resolver.h"
#include "token.h"
#include "transpiler_c.h"

CompilerOptions build_args(StrList* args) {
    if (args->length == 1) {
        print_help: {}
        printf("=== Help ===\n");
        printf("%s <infile.kdo> <out> [options...]\n", args->elements[0]);
        printf("Options:\n");
        printf("    --help       - show this list\n");
        printf("    --run     -r - run the compiled binary\n");
        printf("    --compile -c - compile generated c to executable\n");
        printf("    --raw     -w - do not create wrapper code\n");
        printf("    --cc=<c_compiler_path>\n");
        printf("    --dir=<output_directory>\n");
        printf("    ::lib::path=<path/to/lib_file.kdo> (multiple possible)\n");
        printf("Alternatively use:\n");
        printf("make run file=<infile>\n");
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
    options.module_names = list_new(StrList);
    options.modules = map_new();

    for (usize i = 1;i < args->length;i++) {
        str arg = args->elements[i];
        if (strlen(arg) == 0) continue;
        if (str_startswith(arg, "--")) {
            arg += 2;
            if (str_eq(arg, "help")) {
                goto print_help;
            } else if (str_eq(arg, "run")) {
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
                    list_append(&options.module_names, mod);
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

    if (options.source == NULL) { 
        log("No source file set");
        goto print_help;
    }
    if (options.outname == NULL) { 
        log("No out name set");
        goto print_help;
    }
    if (options.out_dir == NULL) options.out_dir = "./build";
    if (options.c_compiler == NULL) options.c_compiler = "gcc";
    return options;
}

void compile(CompilerOptions options) {
    Program* program = malloc(sizeof(Program));
    program->packages = map_new();

    TokenStream* stream = tokenstream_new(options.source, read_file_to_string(options.source));
    log("Parsing package ::main");
    Module* main = parse_module_contents(stream, gen_path("::main"));
    ModuleItem* main_func_item = map_get(main->items, "main");
    if (main_func_item == NULL || main_func_item->type != MIT_FUNCTION) panic("no main function found");

    Module* intrinsics = gen_intrinsics();
    Module* intrinsics_types = gen_intrinsics_types();

    program->main_module = main;

    insert_module(program, main, true);
    insert_module(program, intrinsics, true);
    insert_module(program, intrinsics_types, true);

    list_foreach(&options.module_names, lambda(void, str modname, {
        str fp = map_get(options.modules, modname);
        str file = to_str_writer(s, fprintf(s, "%s/lib.kdo", fp));
        if (access(file, F_OK) != 0) panic("Could not load package %s: no such file %s", file, modname);
        TokenStream* s = tokenstream_new(file, read_file_to_string(file));
    log("Parsing library package %s", modname);
        Path* modpath = gen_path(modname);
        if (modpath->elements.length != 1) panic("Library path may not be a submodule: expected path to look like ::foo, not %s", modpath);
        if (!modpath->absolute) panic("Library path must be absolute: expected path to look like ::foo, not %s", modpath);
        Module* mod = parse_module_contents(s, modpath);
        mod->filepath = file;
        insert_module(program, mod, true);
    }));

    typedef struct Submodule {
        Path* current;
        str parentfile;
        Identifier* mod;
        bool pub;
    } Submodule;
    LIST(SubList, Submodule);
    SubList sublist = list_new(SubList);
    map_foreach(program->packages, lambda(void, str key, Module* item, {
        UNUSED(key);
        list_foreach(&item->subs, lambda(void, ModDef* m, {({
            if (str_eq(m->name->name, "lib")) spanned_error("Invalid name", m->name->span, "Submodule of %s may not be called lib: lib is a reserved name for toplevel packages", to_str_writer(s, fprint_path(s, item->path)));
            if (item->filepath == NULL) spanned_error("Synthetic", m->name->span, "Synthetic module may not have submodules: %s cannot have submodule %s", 
                    to_str_writer(s, fprint_path(s, item->path)), m->name->name);
            Submodule sm = (Submodule){ .current = item->path, .parentfile=item->filepath, .mod = m->name, .pub = m->pub};
            list_append(&sublist, sm);
        });}));
    }));

    for (usize i = 0;i < sublist.length;i++) {
        Submodule sm = sublist.elements[i];
        Path* modpath = path_new(true, sm.current->elements);
        path_append(modpath, sm.mod);
        str pf = sm.parentfile;
        StrList split = rsplitn(pf, '/', 1);
        str fp = split.elements[0];
        str parent = split.elements[1];
        if (!str_eq(parent, "mod.kdo") && !str_eq(parent, "lib.kdo")) spanned_error("Invalid parent", sm.mod->span, "%s may not register the child module %s as it is located at %s, such is reserved for files named mod.kdo or lib.kdo", to_str_writer(s, fprint_path(s, sm.current)), sm.mod->name, pf);
        str leafname = to_str_writer(s, fprintf(s, "%s/%s.kdo", fp, sm.mod->name));
        str nodename = to_str_writer(s, fprintf(s, "%s/%s/mod.kdo", fp, sm.mod->name));
        bool lx = false;
        bool nx = false;
        if (access(leafname, F_OK) == 0) lx = true;
        if (access(nodename, F_OK) == 0) nx = true;
        if (!lx && !nx) spanned_error("No such module", sm.mod->span, "%s::%s could not be found in %s or %s", to_str_writer(s, fprint_path(s, sm.current)), sm.mod->name, leafname, nodename);
        if (lx && nx) spanned_error("Ambigous module file", sm.mod->span, "Found both %s and %s for %s::%s, remove on of them", leafname, nodename, to_str_writer(s, fprint_path(s, sm.current)), sm.mod->name);
        str file = NULL;
        if (lx) file = leafname;
        if (nx) file = nodename;
        TokenStream* s = tokenstream_new(file, read_file_to_string(file));
        Module* mod = parse_module_contents(s, modpath);
        mod->name = sm.mod;
        mod->filepath = file;
        insert_module(program, mod, sm.pub);
    }

    Module* std = map_get(program->packages, "std");
    map_foreach(intrinsics_types->items, lambda(void, str key, ModuleItem* item, {
        map_put(std->items, key, item);
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

    report_cache();
}