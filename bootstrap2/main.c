#include <stdio.h>
#include <stdlib.h>

#include "lib.h"
#include "compiler.h"       
#include "lib/defines.h"
#include "lib/gc.h"
#include "lib/str.h"

void cleanup_quit(str file, usize line, int code) {
    gc_end();
}

int main(int argc, char* argv[]) {
    init_exit_handler(cleanup_quit);
    gc_begin();

    char* x = gc_malloc(4);
    gc_free(x);

    StrList args = list_new(StrList);
    args.length = argc;
    args.elements = argv;

    CompilerOptions options = build_args(&args);
    compile(options);

    if (options.run) {
        info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "RUN" ANSI_RESET_SEQUENCE, "Running program...");
        i32 code = system(to_str_writer(stream, fprintf(stream, "./%s", options.outname)));
        info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "RUN" ANSI_RESET_SEQUENCE, "Execution finished with code %lu", code);
    }

    quit(0);
}