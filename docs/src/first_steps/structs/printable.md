# Printing II: Printable

Now it is time to make our [`Color`](./structs.md) struct printable.

Here it is again:
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
To advertise its printablility, we need to implement the `Fmt` trait:
```rs
!use std::*;
!use std::fmt::*;
!
!struct Color {
!    r: u8,
!    g: u8,
!    b: u8,
!    a: u8
!}
!
impl Color: Fmt {
    fn dyn_fmt(self: &Color) -> DynFmt { 
        todo()
    }
    fn fmt(self: &Vehicle, fmt: &Formatter, stream: &FormatStream) {
        todo()
    }
}
```

Tne `Fmt` trait has two methods: `fmt` does the formatting and `dyn_fmt` packs the format function together with the object itself together,
since all type information is lost when passing it to the `println` function and otherwise we wouldn't know how to format it

```rs
!use std::*;
!use std::fmt::*;
!
!struct Color {
!    r: u8,
!    g: u8,
!    b: u8,
!    a: u8
!}
!
impl Color: Fmt {
    fn dyn_fmt(self: &Color) -> DynFmt { 
        _ { object: typecast(self), fmt: Color::<>::fmt }
    }
    fn fmt(self: &Color, fmt: &Formatter, stream: &FormatStream) { 
        stream.write_str("Color { r: ").write(fmt, &self.r)
                    .write_str(", g: ").write(fmt, &self.g)
                    .write_str(", b: ").write(fmt, &self.b)
                    .write_str(", a: ").write(fmt, &self.a)
        .write_str(" }");
    }
}
!
!fn main() {
!    let c = Color { r: 255, g: 150, b: 100, a: 255 };
!    println("We have the color $", c.dyn_fmt());
!}
```
>**Note:** We try to avoid using `format` or `formatln` in `fmt`, since that would allocate a string. All the `FormatStream::write*` methods are allocation-free

Now we can comfortably print our `Color` anywhere:
```rs
!use std::*;
!use std::fmt::*;
!
!struct Color {
!    r: u8,
!    g: u8,
!    b: u8,
!    a: u8
!}
!
!impl Color: Fmt {
!    fn dyn_fmt(self: &Color) -> DynFmt { 
!        _ { object: typecast(self), fmt: Color::<>::fmt }
!    }
!    fn fmt(self: &Color, fmt: &Formatter, stream: &FormatStream) { 
!        stream.write_str("Color { r: ").write(fmt, &self.r)
!                    .write_str(", g: ").write(fmt, &self.g)
!                    .write_str(", b: ").write(fmt, &self.b)
!                    .write_str(", a: ").write(fmt, &self.a)
!        .write_str(" }");
!    }
!}
!
fn main() {
    let c = Color { r: 255, g: 150, b: 100, a: 255 };
    println("We have the color $", c.dyn_fmt());
}
```

It is also possible to have more print methods on the same struct, albeit not under the `Fmt` trait any more, which only houses the canonical print

Here we convert the example from the [methods](./methods.md) section to the allocation free print api

```rs
!use std::*;
!use std::fmt::*;
!
!struct Color {
!    r: u8,
!    g: u8,
!    b: u8,
!    a: u8
!}
!
!impl Color: Fmt {
!    fn dyn_fmt(self: &Color) -> DynFmt { 
!        _ { object: typecast(self), fmt: Color::<>::fmt }
!    }
!    fn fmt(self: &Color, fmt: &Formatter, stream: &FormatStream) { 
!        stream.write_str("Color { r: ").write(fmt, &self.r)
!                    .write_str(", g: ").write(fmt, &self.g)
!                    .write_str(", b: ").write(fmt, &self.b)
!                    .write_str(", a: ").write(fmt, &self.a)
!        .write_str(" }");
!    }
!}
!
impl Color {
    fn dyn_ansi_fg(self: &Color) -> DynFmt { 
        _ { object: typecast(self), fmt: Color::<>::ansi_fg_fmt }
    }
    fn ansi_fg_fmt(self: &Color, fmt: &Formatter, stream: &FormatStream) { 
        stream.write_str("\x1b[38;2;").write(fmt, &self.r)
                       .write_str(";").write(fmt, &self.g)
                       .write_str(";").write(fmt, &self.b)
        .write_str("m");
    }
    fn ansi_reset() -> DynFmt {
        // dummy object since this is a static method
        _ { object: c_api::null, fmt: Color::<>::ansi_reset_fmt }
    }
    fn ansi_reset_fmt(self: &Color, fmt: &Formatter, stream: &FormatStream) {
        // Note: do not access `self` since its a null dummy value 
        stream.write_str("\x1b[0m");
    }
}

fn main() {
    let c = Color { r: 255, g: 150, b: 100, a: 255 };
    println("We have the color $", c.dyn_fmt());
    println("Look, $colorful text$!\n", c.dyn_ansi_fg(), Color::<>::ansi_reset());
}
```
