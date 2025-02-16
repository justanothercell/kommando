# First steps

Let's start by writing a hello world program:
```rs
use std::*;

fn main() {
    println("Hello, world");
}
```
Save this into a file called `hello.kdo`

Although this seems like not much, it already uses a bunch of language features and with it a bunch of libraries:
- [core](https://github.com/justanothercell/kommando/tree/dev/kdolib/core) library: every project needs this library as it defines core language primitives
- [std](https://github.com/justanothercell/kommando/tree/dev/kdolib/std) library: this library defines `println` and a bunch of other useful functions and structures
- [c_api](https://github.com/justanothercell/kommando/tree/dev/kdolib/c_api) library: used in-proxy by `std`, it defines many c ffi functions.

To compile this example we need to provide path to these libraries, located in the [kdolib/](https://github.com/justanothercell/kommando/tree/dev/kdolib) directory.

>**Note:** Use `make help` or `./kommando --help` to get a complete list of options 

Here we assume we are in the root directory of the [kommando](https://github.com/justanothercell/kommando/tree/dev) compiler, otherwise you need to adjust the paths in the command.

```sh
./kommando hello.kdo hello -cr ::core=kdolib/core ::core=kdolib/c_api ::core=kdolib/std 
```

>**Note:** Command line arguments are generally order-independant

Since this is rather verbose and inclusion of these libraries is common, there is an automatic inclusion script:

```sh
./kommando hello.kdo hello -cr $(./kdolib/link)
```

For even more convenience, use the included Makefile instead:

```sh
make run file=hello.kdo
```

>**Note:** Use `make compile` or `-c` to compile without running. 