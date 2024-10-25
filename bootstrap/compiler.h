#ifndef COMPILER_H
#define COMPILER_H

#include "lib.h"
#include "module.h"
#include "token.h"

typedef struct CompilerOptions {
    str source;
    str outname;
    str out_dir;
    str c_compiler;
    bool run;
    bool raw;
    bool compile;
    Map* modules;
} CompilerOptions;

CompilerOptions build_args(StrList* args);
void compile(CompilerOptions options);
 
#endif