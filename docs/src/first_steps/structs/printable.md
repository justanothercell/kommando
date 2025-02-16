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
    fn dyn_fmt(self: ptr<Color>) -> DynFmt { todo() }
    fn fmt(self: ptr<Vehicle>, fmt: ptr<Formatter>, stream: ptr<FormatStream>) { todo() }
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
    fn dyn_fmt(self: ptr<Color>) -> DynFmt { 
        _ { object: typecast(self), fmt: Color::<>::fmt }
    }
    fn fmt(self: ptr<Color>, fmt: ptr<Formatter>, stream: ptr<FormatStream>) { 
        stream.write_str("Color { r: ").write(fmt, self.r)
                    .write_str(", g: ").write(fmt, self.g)
                    .write_str(", b: ").write(fmt, self.b)
                    .write_str(", a: ").write(fmt, self.a)
        .write_str(" }");
    }
}
!
!fn main() {
!    let c = Color { r: 255, g: 150, b: 100 };
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
!    fn dyn_fmt(self: ptr<Color>) -> DynFmt { 
!        _ { object: typecast(self), fmt: Color::<>::fmt }
!    }
!    fn fmt(self: ptr<Color>, fmt: ptr<Formatter>, stream: ptr<FormatStream>) { 
!        stream.write_str("Color { r: ").write(fmt, self.r)
!                    .write_str(", g: ").write(fmt, self.g)
!                    .write_str(", b: ").write(fmt, self.b)
!                    .write_str(", a: ").write(fmt, self.a)
!        .write_str(" }");
!    }
!}
!
fn main() {
    let c = Color { r: 255, g: 150, b: 100 };
    println("We have the color $", c.dyn_fmt());
}
```