# Kommando Programming Language [*.kdo]

## Quickstart
```sh
cd bootstrap2/build
gcc -Wall -Wextra -pedantic ../main.c ../ast.c ../infer.c ../tokens.c ../transpile.c ../types.c ../lib/str.c ../lib/defines.c -o compiler
compiler.exe --run --main --dir=build test.kdo test
```
Note that this is meant to be compiled with gcc and uses certain compiler extensions. There exists a [./bootstrap2/.clangd](./bootstrap2/.clangd) file,
but that is only to fix VSCode errors/hints. Specifically, the defined macro `__INTELLISENSE__` temporarily removes inline functions from
the `LAMBDA` macro in [./bootstrap2/lib/defines.h](./bootstrap2/lib/defines.h) and as such cannot be used for compilation.