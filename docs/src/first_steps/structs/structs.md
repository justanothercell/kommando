# Structs
A struct can combine small types to a bigger type. Here four `u8`s represent an RGBA color:
```rs
!use std::*;
!
struct Color {
    r: u8,
    g: u8,
    b: u8,
    a: u8
}
```
You then can instanciate the struct using literal syntax:
```rs
!use std::*;
!
!struct Color {
!    r: u8,
!    g: u8,
!    b: u8,
!    a: u8
!}
!
!fn main() {
let c = Color { r: 255, g: 150, b: 180, a: 255 };
!}
```
You can also modify the fields:
```rs
!use std::*;
!
!struct Color {
!    r: u8,
!    g: u8,
!    b: u8,
!    a: u8
!}
!
!fn main() {
!let c = Color { r: 255, g: 150, b: 180, a: 255 };
c.r /= 2;
c_api::printf("Color { r: %u, g: %u, b: %u, a: %u }\n", c.r, c.g, c.b, c.a);
!}
```

>**Note:** [Later](./printable.md) we will use the `Fmt` trait to automatically format the color for printing

Pointers to structs automatically dereference on field access:
```rs
!use std::*;
!
!struct Color {
!    r: u8,
!    g: u8,
!    b: u8,
!    a: u8
!}
!
!fn main() {
!let c = Color { r: 255, g: 150, b: 180, a: 255 };
let c_ref: ptr<Color> = &c; // points to c
(*c_ref).r /= 2; // manually dereference to access the inner value of c_ref
// ...or let the compiler do so automatically:
c_api::printf("Color { r: %u, g: %u, b: %u, a: %u }\n", c_ref.r, c_ref.g, c_ref.b, c_ref.a);
!}
```