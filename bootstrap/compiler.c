#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lib.h"
#include "lib/exit.h"
#include "lib/list.h"
#include "lib/str.h"
LIB;
#include "compiler.h"
#include "ast.h"
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
        printf("    --silent     - verbosity = 0\n");
        printf("    --static     - staticmay link libraries\n");
        printf("    --verbose -v - verbosity += 1 (default = 1)\n");
        printf("    --trace=[none|main|all]\n");
        printf("             none - do not generate traceback info\n");
        printf("             this - (default) generate traceback info for the main package\n");
        printf("             full - also generate traceback info for libraries\n");
        printf("    --cc=<c_compiler_path>\n");
        printf("    --dir=<output_directory>\n");
        printf("    ::package=<path/to/package> (multiple possible)\n");
        printf("    --include=path/to/c_header.h (multiple possible)\n");
        printf("The following options are compiler diagnostics:\n");
        printf("    --trace-compiler - trace the compiler on panic\n");
        printf("    --emit-spans     - Write source references as comments in generated c\n");
        printf("    --log-lines      - Include [source.c:line] in console logs\n");
        printf("Using `make` with sensibe defaults:\n");
        printf("make run file=<infile> flags=\"<optional compiler flags>\"\n");
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
    options.emit_spans = false;
    options.package_names = list_new(StrList);
    options.c_headers = list_new(StrList);
    options.packages = map_new();
    options.verbosity = 1;
    options.tracelevel = 1;

    for (usize i = 1;i < args->length;i++) {
        str arg = args->elements[i];
        if (strlen(arg) == 0) continue;
        if (str_startswith(arg, "--")) {
            arg += 2;
            if (str_eq(arg, "help")) {
                goto print_help;
            } else if (str_eq(arg, "run")) {
                options.run = true;
            } else if (str_eq(arg, "trace-compiler")) {
                TRACE_ON_PANIC = true;
            } else if (str_eq(arg, "log-lines")) {
                LOG_LINES = true;
            } else if (str_eq(arg, "emit-spans")) {
                options.emit_spans = true;
            } else if (str_eq(arg, "compile")) {
                options.compile = true;
            } else if (str_eq(arg, "silent")) {
                options.verbosity = 0;
            } else if (str_eq(arg, "static")) {
                options.static_links = true;
            } else if (str_startswith(arg, "trace=")) {
                arg += 6;
                if (str_eq(arg, "none")) {
                    options.tracelevel = 0;
                } else if (str_eq(arg, "this")) {
                    options.tracelevel = 1;
                } else if (str_eq(arg, "full")) {
                    options.tracelevel = 2;
                } else {
                    log(ANSI(ANSI_RED_FG, ANSI_BOLD) "Error: " ANSI_RESET_SEQUENCE "Invalid trace level `--trace=%s`", arg);
                    goto print_help;
                }
            } else if (str_startswith(arg, "include=")){
                list_append(&options.c_headers, arg+8);
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
                } else if (arg[j] == 'v') {
                    options.verbosity += 1;
                } else {
                    log(ANSI(ANSI_RED_FG, ANSI_BOLD) "Error: " ANSI_RESET_SEQUENCE "Invalid flag `%c` in `-%s`", arg[j], arg);
                    goto print_help;
                }
            } 
        } else if (str_startswith(arg, "::")) {
            for (usize i = 2;i < strlen(arg);i++) {
                if (arg[i] == '=') {
                    str package = to_str_writer(stream, fprintf(stream, "%.*s", (int)(i-2), arg+2));
                    str path = arg + i + 1;
                    map_put(options.packages, package, path);
                    list_append(&options.package_names, package);
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

typedef struct Submodule {
    Path* current;
    str parentfile;
    Identifier* mod;
    Visibility vis;
} Submodule;
LIST(SubList, Submodule);

void compile(CompilerOptions options) {
    Program* program = malloc(sizeof(Program));
    program->packages = map_new();

    if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "PARSER" ANSI_RESET_SEQUENCE, "Parsing source files...");
    TokenStream* stream = tokenstream_new(options.source, read_file_to_string(options.source));
    if (options.verbosity >= 2) log("Parsing package ::main");
    Module* main = parse_module_contents(stream, gen_path("::main"));
    ModuleItem* main_func_item = map_get(main->items, "main");
    if (main_func_item == NULL || main_func_item->type != MIT_FUNCTION) panic("no main function found");

    program->main_module = main;

    insert_module(program, &options, main, V_PUBLIC);

    list_foreach(&options.package_names, i, str package_name, {
        str fp = map_get(options.packages, package_name);
        str file = to_str_writer(s, fprintf(s, "%s/lib.kdo", fp));
        if (access(file, F_OK) != 0) panic("Could not load package %s: no such file %s", file, package_name);
        TokenStream* s = tokenstream_new(file, read_file_to_string(file));
        if (options.verbosity >= 2) log("Parsing library package %s", package_name);
        Path* modpath = gen_path(to_str_writer(s, fprintf(s, "::%s", package_name)));
        if (modpath->elements.length != 1) panic("Library path may not be a submodule: expected path to look like ::foo, not %s", modpath);
        Module* package = parse_module_contents(s, modpath);
        package->filepath = file;
        insert_module(program, &options, package, V_PUBLIC);
        SubList sublist = list_new(SubList);
        list_foreach(&package->subs, i, ModDef* m, ({
            if (str_eq(m->name->name, "lib")) spanned_error("Invalid name", m->name->span, "Submodule of %s may not be called lib: lib is a reserved name for toplevel packages", to_str_writer(s, fprint_path(s, package->path)));
            if (package->filepath == NULL) spanned_error("Synthetic module error", m->name->span, "Synthetic module may not have submodules: %s cannot have submodule %s", 
                    to_str_writer(s, fprint_path(s, package->path)), m->name->name);
            Submodule sm = (Submodule){ .current = package->path, .parentfile=package->filepath, .mod = m->name, .vis = m->vis};
            list_append(&sublist, sm);
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
            insert_module(program, &options, mod, sm.vis);
            list_foreach(&mod->subs, i, ModDef* m, ({
                if (str_eq(m->name->name, "lib")) spanned_error("Invalid name", m->name->span, "Submodule of %s may not be called lib: lib is a reserved name for toplevel packages", to_str_writer(s, fprint_path(s, package->path)));
                if (mod->filepath == NULL) spanned_error("Synthetic module error", m->name->span, "Synthetic module may not have submodules: %s cannot have submodule %s", 
                        to_str_writer(s, fprint_path(s, package->path)), m->name->name);
                Submodule sm = (Submodule){ .current = package->path, .parentfile=package->filepath, .mod = m->name, .vis = m->vis};
                list_append(&sublist, sm);
            }));
        }
    });

    if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "RESOLVER" ANSI_RESET_SEQUENCE, "Resolving types...");
    resolve(program, &options);

    if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "TRANSPILER" ANSI_RESET_SEQUENCE, "Transpiling to c code...");
    str code_file_name = to_str_writer(stream, fprintf(stream, "%s.c", options.outname));
    str header_file_name = to_str_writer(stream, fprintf(stream, "%s.h", options.outname));
    FILE* code_file = fopen(code_file_name, "w");
    FILE* header_file = fopen(header_file_name, "w");
    transpile_to_c(program, &options, header_file, code_file, header_file_name);
    fclose(header_file);
    fclose(code_file);

    if (options.verbosity >= 1) report_item_cache_stats();

    if (options.compile) {
        if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "COMPILE_C" ANSI_RESET_SEQUENCE, "Compiling generated c code...");
        str command = to_str_writer(stream, fprintf(stream, "%s -ggdb -Wall -Wno-unused -Wno-builtin-declaration-mismatch %s -lm -o %s %s", 
            options.c_compiler, code_file_name, options.outname, options.static_links ? "-static" : ""));
        fflush(stdout);
        i32 r = system(command);
        if (r != 0) panic("%s failed with error code %lu", options.c_compiler, WEXITSTATUS(r));
    }
    if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "CONPILER" ANSI_RESET_SEQUENCE, "Compilation finished!");
}