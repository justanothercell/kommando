#include <stdio.h>
#include <stdlib.h>

#include "lib.h"
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
        info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "RUN" ANSI_RESET_SEQUENCE, "Running " ANSI(ANSI_WHITE_FG) "%s" ANSI_RESET_SEQUENCE " ...", options.source);
        i32 code = system(to_str_writer(stream, fprintf(stream, "./%s", options.outname)));
        info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "RUN" ANSI_RESET_SEQUENCE, "Execution finished with code %lu", code);
    }

    quit(0);
}