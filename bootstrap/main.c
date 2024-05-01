#include "tokens.c"
#include "ast.c"
#include "transpile.c"
#include "lib/defines.c"

#ifdef __WIN32__
    #include <io.h>
#endif

#include <stdio.h>
#include <string.h>
#include "lib/os.c"
#include "types.c"

int main(int argc, char** argv) {
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

    char* INPUT_FILE = NULL;
    char* OUTNAME = NULL;
    char* OUT_DIR = ".";

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

    char* OUT_C_FILE = malloc(strlen(argv[1] + 2));
    sprintf(OUT_C_FILE, "%s.c", OUTNAME);
    char* OUT_H_FILE = malloc(strlen(argv[1] + 2));
    sprintf(OUT_H_FILE, "%s.h", OUTNAME);

    FILE *source = fopen(INPUT_FILE, "r");
    TokenStream* stream = new_tokenstream(source);
    Module* module = parse_module(stream);
    drop_tokenstream(stream);

    usize tlen = module->types_c;
    usize types_capacity = module->types_c;
    register_builtin_types(&module->types, &module->types_c, &types_capacity);
    printf("registered %lld builtins\n", module->types_c - tlen);
    
    mkdir(OUT_DIR);
    if (chdir("build") != 0) {
        printf("`cd build` failed\n");
        exit(1);
    }

    FILE *code = fopen(OUT_C_FILE, "w");
    FILE *header = fopen(OUT_H_FILE, "w");
    Writer* writer = new_writer(OUTNAME, code, header, gen_main);
    transpile_module(writer, module);
    finalize_transpile(writer);
    drop_writer(writer);

    if (to_exe) {
        printf("compiling generated c...\n");
        
#ifdef __WIN32__
        const char* BUILD_CMD_TEMPLATE = "gcc %s -Wall -o %s.exe";
        char* command = malloc(sizeof(char) * (strlen(BUILD_CMD_TEMPLATE) + strlen(OUT_C_FILE) + strlen(OUTNAME) + 1));
        sprintf(command, BUILD_CMD_TEMPLATE, OUT_C_FILE, OUTNAME);
#else
        const char* BUILD_CMD_TEMPLATE = "gcc %s -Wall -o %s";
        char* command = malloc(sizeof(char) * (strlen(BUILD_CMD_TEMPLATE) + strlen(OUT_C_FILE) + strlen(OUTNAME) + 1));
        sprintf(command, BUILD_CMD_TEMPLATE, OUT_C_FILE, OUTNAME);
#endif
        if (system(command) != 0) {
            printf("Compilation failed\n");
            exit(1);
        }

        printf("Compilation finished successfully.\n");
    }
    if (run) {
        int return_code = system(OUTNAME);
        printf("Program exited with code %d.", return_code);
    }
}