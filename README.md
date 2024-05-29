# Kommando Programming Language [*.kdo]

## Quickstart
```sh
cd bootstrap/build
gcc -Wall -Wextra -pedantic ../main.c ../ast.c ../infer.c ../tokens.c ../transpile.c ../types.c ../lib/str.c ../lib/defines.c -o compiler
compiler.exe --run --main --dir=build test.kdo test
```