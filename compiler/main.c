#pragma once

#include "tokens.c"
#include "ast.c"
#include "transpile.c"
#include "lib/defines.c"
#include <stdio.h>

int main(int argc, char** argv)
{
    FILE *source = fopen("test.kdo", "r");
    TokenStream* stream = new_tokenstream(source);
    Module* module = parse_module(stream);
    drop_tokenstream(stream);
    FILE *code = fopen("test.c", "w");
    FILE *header = fopen("test.h", "w");
    Writer* writer = new_writer("test", code, header);
    transpile_module(writer, module);
    drop_writer(writer);
    printf("goodbye o/\n");
}