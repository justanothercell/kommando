#ifndef COMPILER_H
#define COMPILER_H

#include "lib.h"
#include "module.h"
#include "token.h"

typedef struct CompilerOptions {
    str source;
    str outname;
    str out_dir;
    bool run;
    bool raw;
    bool compile;
} CompilerOptions;

CompilerOptions build_args(StrList* args);
void compile(str file, str source);
 
#endif
