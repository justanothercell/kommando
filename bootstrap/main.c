#include <stdio.h>
#include <stdlib.h>

#include "lib.h"
LIB;
#include "compiler.h"

void cleanup_quit(str file, usize line, int code) {
    UNUSED(file); UNUSED(line); UNUSED(code);
}

int main(int argc, char* argv[]) {
    init_exit_handler(cleanup_quit);

    char* x = malloc(4);
    free(x);

    StrList args = list_new(StrList);
    args.length = argc;
    args.elements = argv;

    CompilerOptions options = build_args(&args);
    compile(options);

    if (options.run) {
        if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_CYAN_FG) "RUN" ANSI_RESET_SEQUENCE, "Running " ANSI(ANSI_WHITE_FG) "%s" ANSI_RESET_SEQUENCE " ...", options.source);
        fflush(stdout);
        i32 code = system(options.outname);
        if (code == -1) {
            if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_RED_FG) "RUN" ANSI_RESET_SEQUENCE, "Could not start execution");
        } else {
            if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_CYAN_FG) "RUN" ANSI_RESET_SEQUENCE, "Execution finished with code %ld", WEXITSTATUS(code));
        }
    }

    quit(0);
}