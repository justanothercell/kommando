# Variables
Variables can be created using the following syntax:
```rs
let <name>: <Type> = <value>;
```
Here we create the variable `count` as a signed 32 bit int and initialize it with the value `5`:
```rs
!use std::*;
!
!fn main() {
let count: i32 = 5;
!}
```
Type inferrence can be used to obtain the type from surrounding context:
```rs
!use std::*;
!
!fn main() {
let count: _ = 5;
!}
```
The `_` is a placeholder and can stand for any type.

If such type inferrence is successful, we can also omit the type fully:
```rs
!use std::*;
!
!fn main() {
let count = 5;
!}
```
Integer constants may also have different types, which can again be inferred from both sides:
```rs
!use std::*;
!
!fn main() {
let a = 5usize;
let b: usize = 5;
let c: u8 = 5i8; // This does not work as the two types conflict!
!}
```