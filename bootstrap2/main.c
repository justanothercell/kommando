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
        system(to_str_writer(stream, fprintf(stream, "./%s", options.outname)));
    }

    quit(0);
}