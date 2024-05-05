#include "lib/defines.h"
#include "lib/os.h"

#include "lib/str.c"

#ifdef __WIN32__
    #include <io.h>
#endif

#include "ast.h"
#include "infer.h"
#include "tokens.h"
#include "transpile.h"
#include "types.h"

#include <stdio.h>
#include <string.h>


int main(int argc, str* argv) {
    if (argc < 3) {
        printf("Usage:\n [--exe|-e|--main|-m|--run|-r|--dir|-d] %s <input.kdo> <output>]\n\
    --exe |-e - compile to executable\n\
    --main|-m - wrap main in c style main\n\
    --run |-r - run the executable, implies --exe\n\
    --dir |-d - the artifact output directory, default `-d=.`\n\
        ", argv[0]);
        exit(1);
    }
    bool to_exe = false;
    bool gen_main = false;
    bool run = false;

    str INPUT_FILE = NULL;
    str OUTNAME = NULL;
    str OUT_DIR = ".";

    for (int i = 1;i < argc;i++) {
        if (strlen(argv[i]) == 0) continue; 
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--exe") == 0 || strcmp(argv[i], "-e") == 0) to_exe = true;
            else if (strcmp(argv[i], "--main") == 0 || strcmp(argv[i], "-m") == 0) gen_main = true;
            else if (strcmp(argv[i], "--run") == 0 || strcmp(argv[i], "-r") == 0) run = true;
            else if (strncmp(argv[i], "--dir=", strlen("--dir=")) == 0) {
                usize l = strlen(argv[i] + strlen("--dir="));
                OUT_DIR = malloc((l + 1) * sizeof(char));
                strcpy(OUT_DIR, argv[i] + strlen("--dir="));
                OUT_DIR[l] = '\0';
            }
            else {
                printf("Invalid flag `%s` from [--exe|-e|--main|-m|--run|-r|--dir|-d]\n", argv[i]);
                exit(1);
            }
        } else {
            if (INPUT_FILE == NULL) INPUT_FILE = argv[i];
            else if (OUTNAME == NULL) OUTNAME = argv[i];
            else {
                printf("Invalid parameter `%s`\n", argv[i]);
                exit(1);
            }
        }
    }

    to_exe |= run;

    printf("compilign with flags: exe=%u, main=%u, run=%u\n", to_exe, gen_main, run);

    str OUT_C_FILE = malloc(strlen(argv[1] + 2));
    sprintf(OUT_C_FILE, "%s.c", OUTNAME);
    str OUT_H_FILE = malloc(strlen(argv[1] + 2));
    sprintf(OUT_H_FILE, "%s.h", OUTNAME);

    FILE *source = fopen(INPUT_FILE, "r");
    TokenStream* stream = new_tokenstream(source);
    Module* module = parse_module(stream);
    drop_tokenstream(stream);

    usize tlen = module->types.length;
    register_builtin_types(&module->types);
    printf("registered %lld builtins\n", module->types.length - tlen);
    
    printf("type pass...\n");
    infer_types(module);
    printf("inferred and propagated types\n");

    mkdir(OUT_DIR);
    if (chdir("build") != 0) {
        free(OUT_DIR);
        printf("`cd build` failed\n");
        exit(1);
    }
    free(OUT_DIR);

    FILE *code = fopen(OUT_C_FILE, "w");
    FILE *header = fopen(OUT_H_FILE, "w");
    Writer* writer = new_writer(OUTNAME, code, header, gen_main);
    transpile_module(writer, module);
    finalize_transpile(writer);
    drop_writer(writer);
    drop_module(module);
    drop_temp_types();

    if (to_exe) {
        printf("compiling generated c...\n");
        
#ifdef __WIN32__
        const str BUILD_CMD_TEMPLATE = "gcc %s -Wall -o %s.exe";
        str command = malloc(sizeof(char) * (strlen(BUILD_CMD_TEMPLATE) + strlen(OUT_C_FILE) + strlen(OUTNAME) + 1));
        sprintf(command, BUILD_CMD_TEMPLATE, OUT_C_FILE, OUTNAME);
        free(OUT_C_FILE);
        free(OUT_H_FILE);
#else
        const str BUILD_CMD_TEMPLATE = "gcc %s -Wall -o %s";
        str command = malloc(sizeof(char) * (strlen(BUILD_CMD_TEMPLATE) + strlen(OUT_C_FILE) + strlen(OUTNAME) + 1));
        sprintf(command, BUILD_CMD_TEMPLATE, OUT_C_FILE, OUTNAME);
#endif
        if (system(command) != 0) {
            free(command);
            printf("compilation failed\n");
            exit(1);
        }
        free(command);

        printf("compilation finished successfully.\n");
    }
    if (run) {
        int return_code = system(OUTNAME);
        printf("program exited with code %d.\n", return_code);
    }
}