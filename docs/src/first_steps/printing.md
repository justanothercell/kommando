# Printing

There are two main ways of formatting and printing, depending on your needs.

## The C-style way
Using posix `printf`, we can do C-style formats:
```rs
!use std::*;
!
!fn main() {
c_api::printf("int: %d, string: %s, pointer: %p\n", 
    4, "foo", &unit {}
);
!}
```
Text can also be formatted into a string instead of being printed directly:
```rs
!use std::*;
!
!fn main() {
let text: c_str = c_api::formatf("Hello, %s\n", "world");
c_api::printf("%s", text);
!}
```

## The formatter way
Using traits, we can format a lot more than just primitives:
```rs
!use std::*;
!
!fn main() {
println("int: $, string: $, Option: $", 
    42.dyn_fmt(),
    "foo".dyn_fmt(),
    Option::<u8>::some(3).dyn_fmt()
);
!}
```
>**Note:** Since our strign no longer contaisn the necessary information to deduce the type, we need to package the value together with it's formatting function. Because of that, every format argument must be turned into `DynFmt` as seen above. Failing to do so will most likely crash the program, the same way an invalid C-style format string would cause a segfault.<br>
You can also implement formatting for your [own types](https://github.com/justanothercell/kommando/tree/dev/examples/api/format.kdo)
```rs
!use std::*;
!
!fn main() {
// segfaults as a an int cannot function as a string
c_api::printf("%s", 4);
!}
```
Text can also be formatted into a string instead of being printed directly:
```rs
!use std::*;
!
!fn main() {
// use `formatln` to get a newline appended or just `format` without this feature
let text: c_str = formatln("Hello, $", "world".dyn_fmt());
println(text);
!}
```