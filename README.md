# Kommando Programming Language [*.kdo]

## Quickstart
```sh
cd bootstrap2/build
./build.sh # build the compiler
./compiler $(./link_kdolib) hello.kdo hello -cr # compile an run hello.kdo
```
Note that this is meant to be compiled with gcc and uses certain gnu compiler extensions. There exists a [./bootstrap2/.clangd](./bootstrap2/.clangd) file,
but that is only to fix VSCode errors/hints. Specifically, the defined macro `__INTELLISENSE__` temporarily removes inline functions from
the `lambda` macro in [./bootstrap2/lib/defines.h](./bootstrap2/lib/defines.h) and as such cannot be used for compilation.