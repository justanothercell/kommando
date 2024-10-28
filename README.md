```
 __  _   ___   ___ ___  ___ ___   ____  ____   ___     ___  
|  |/ ] /   \ |   |   ||   |   | /    ||    \ |   \   /   \       ______________
|  ' / |     || _   _ || _   _ ||  o  ||  _  ||    \ |     |     |[]            |
|    \ |  O  ||  \_/  ||  \_/  ||     ||  |  ||  D  ||  O  |     |  __________  |
|     ||     ||   |   ||   |   ||  _  ||  |  ||     ||     |     |  | *.kdo  |  |
|  .  ||     ||   |   ||   |   ||  |  ||  |  ||     ||     |     |  |        |  |
|__|\_| \___/ |___|___||___|___||__|__||__|__||_____| \___/      |  |________|  |
 _  _  _  __ _  _       ___    __       _     __    _  __ __     |   ________   |
|_)|_)/ \/__|_)|_||V||V| | |\|/__   |  |_||\|/__| ||_|/__|_      |   [ [ ]  ]   |
|  | \\_/\_|| \| || || |_|_| |\_|   |__| || |\_||_|| |\_||__     \___[_[_]__]___|
```

## Quickstart
```sh
./build # build the compiler
./run examples/hello # compile an run examples/hello.kdo

# to do both steps in one, especially useful during development
./cr examples/hello

# you can also run the compiler manually (once it is compiled itself)
./kommando $(./kdolib/link) examples/hello.kdo examples/hello -cr

# run witnout any arguments to see a list of options
./kommando
```
Note that this is meant to be compiled with gcc and uses certain gnu compiler extensions. There exists a [./bootstrap/.clangd](./bootstrap/.clangd) file,
but that is only to fix VSCode errors/hints. Specifically, the defined macro `__INTELLISENSE__` temporarily removes inline functions from
the `lambda` macro in [./bootstrap/lib/defines.h](./bootstrap/lib/defines.h) and as such cannot be used for compilation.