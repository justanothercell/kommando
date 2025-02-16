# No-std

To get more of an understanding for this process, let's try building the same program, but without the use of the standard library.

Fist we start by including all the core primitive types
```rs
use core::types::*;
```

Then we define the extern c function that we want to call.

The c posix function `int puts(const char *s);` from `stdio.h` takes a `char*` (here `c_str`) as input 
and returns a signed 32 bit `int` (here `i32`)

```rs
!use core::types::*;
!
#[extern]
fn puts(s: c_str) -> i32;
```

Now we are ready to call the function in main:

```rs
!use core::types::*;
!
!#[extern]
!fn puts(s: c_str) -> i32;
!
fn main() {
    puts("Hello, world!\n");
}
```

Save this file as `no_std.kdo` and compile it:

>**Note:** We only need to include the `core` library this time

```sh
./kommando no_std.kdo no_std ::core=kdolib/core -cr
```

Or, with the use of the link file:

```sh
./kommando no_std.kdo no_std $(kdolib/link_core) -cr
```