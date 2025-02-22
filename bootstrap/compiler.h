#ifndef COMPILER_H
#define COMPILER_H

#include "lib.h"
#include "lib/str.h"
#include "module.h"
#include "token.h"
#include "parser.h"

typedef struct CompilerOptions {
    str source;
    str outname;
    str out_dir;
    str c_compiler;
    bool run;
    bool raw;
    bool compile;
    bool static_links;
    bool emit_spans;
    StrList package_names;
    Map* packages;
    int verbosity;
    int tracelevel;
    StrList c_headers;
} CompilerOptions;

CompilerOptions build_args(StrList* args);
void compile(CompilerOptions options);
 
#endif
