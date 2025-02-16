# Methods

Methods for the most part function like normal functions:

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
impl Color {
    fn ansi_fg(c: ptr<Color>) -> c_str {
        c_api::formatf("\x1b[38;2;%u;%u;%um", c.r, c.g, c.b)
    }

    fn ansi_reset() -> c_str {
        "\x1b[0m"
    }
}
!
!fn main() {
!    let c = Color { r: 255, g: 150, b: 180, a: 255 };
!    c_api::printf("Look, %scolorful text%s!\n", Color::<>::ansi_fg(&c), Color::<>::ansi_reset());
!}
```
>**Note:** `ansi_fg` allocates memory! [Later](printable.md) we will use an allocation-free method using `Fmt` and the modern print api which can be adapted to this example

To invoke the method we insert `<>` to disambiguate it from a function in another module:
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
!impl Color {
!    fn ansi_fg(c: ptr<Color>) -> c_str {
!        c_api::formatf("\x1b[38;2;%u;%u;%um", c.r, c.g, c.b)
!    }
!
!    fn ansi_reset() -> c_str {
!        "\x1b[0m"
!    }
!}
!
fn main() {
    let c = Color { r: 255, g: 150, b: 180, a: 255 };
    c_api::printf("Look, %scolorful text%s!\n", 
        Color::<>::ansi_fg(&c), Color::<>::ansi_reset()
    );
}
```
Is the first parameter of a method `Color` or `ptr<Color>`, we can also use the direct method syntax:
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
!impl Color {
!    fn ansi_fg(c: ptr<Color>) -> c_str {
!        c_api::formatf("\x1b[38;2;%u;%u;%um", c.r, c.g, c.b)
!    }
!
!    fn ansi_reset() -> c_str {
!        "\x1b[0m"
!    }
!}
!
fn main() {
    let c = Color { r: 255, g: 150, b: 180, a: 255 };
    c_api::printf("Look, %scolorful text%s!\n", 
        c.ansi_fg(), Color::<>::ansi_reset()
    );
}
```