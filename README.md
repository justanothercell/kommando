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

> Take a look at the online [Sandbox](http://callie.cat) and the [docs](http://callie.cat/docs)

## Quickstart
```sh
make build
make run file=examples/hello

# or both in one step:
make br file=examples/hello

# you can also run the compiler manually (once it is compiled itself).
# ./kdolib/link is a bash file which inserts the kommando library paths
./kommando $(./kdolib/link) examples/hello.kdo examples/hello -cr

# run witout any arguments to see a list of options
./kommando
```
## Examples
```rs
use core::types::*;

#[extern]
fn printf(fmt: c_str, ...) -> i32; 

// this is compilable with 
// ./kommando -cr examples/misc/no_std.kdo examples/misc/no_std ::core=kdolib/core
// without the use of any libraries except the core library.
fn main() {
    printf("Hello, world\n");
}
```
[more examples](<https://github.com/justanothercell/kommando/tree/master/examples>) | [postfix expression calculator](<https://github.com/justanothercell/kommando/blob/master/examples/calculator/calculator.kdo>)
## Notable features
- structs
- [generics](<https://github.com/justanothercell/kommando/tree/master/examples/lang/generics.kdo>)
- [traits](<https://github.com/justanothercell/kommando/tree/master/examples/lang/traits.kdo>) and [methods](<https://github.com/justanothercell/kommando/tree/master/examples/lang/methods.kdo>)
- c ffi for both export and import, aswell as precise control over name mangling 
- modules and [packages/libraries](<https://github.com/justanothercell/kommando/tree/master/kdolib>)
- [constants and globals](<https://github.com/justanothercell/kommando/tree/master/examples/lang/globals.kdo>)
---
Note that this is meant to be compiled with gcc and uses certain gnu compiler extensions. There exists a [./bootstrap/.clangd](./bootstrap/.clangd) file,
but that is only to fix VSCode linting. Specifically, the macro `__INTELLISENSE__` disables these gcc extensions. Other compilers might work but support is not guaranteed.