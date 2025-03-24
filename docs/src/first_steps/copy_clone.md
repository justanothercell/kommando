# To Copy or not to Copy (or to Clone)

Take a look at the following code:
```rs
!use std::*;
!
fn main() {
    let a = 4;
    let b = a;
}
```
As we learned in the chapter about [pointers](./pointers.md), `a` and `b` are now (althozgh [sic] with same value) independent objects.

However, this is not always so clear cut:
```rs
!use std::*;
!
struct Foo {
    x: i32,
    r: &i32
}

fn main() {
    let v = 4;
    let a = Foo { x: 5, r: &v };
    let b = a;
}
```

While `Foo` itself gets copied with its fields `r` and `x`, the underlying value of that reference stays the same and as such both `a.r` and `b.r` point to the same value and as such a and b are in some way linked!<br>
However, `a.x` and `b.x` are totally unlinked!

Having such a partially linked struct, while maybe desireable, would in most circumstances be unwanted or confusing.

Therefore in this instance, `a` is not actually _copied_ into `b`, but _moved_ thereinto and accessing `a` after this would result in a compiler error.

```rs
!use std::*;
!
!struct Foo {
!    x: i32,
!    r: &i32
!}
!
fn main() {
    let v = 4;
    let a = Foo { x: 5, r: &v };
    let b = a;
    let x = a.x; // Compiler error: value of a was already moved
}
```

There are 3 way of resolving this issue, dependign on whether you want linake or not:

## 1. Taking a reference (full link)
This is recommended when you actually want to work on the same value, i.e. when passing to a method
```rs
!use std::*;
!
!struct Foo {
!    x: i32,
!    r: &i32
!}
!
fn main() {
    let v = 4;
    let a = Foo { x: 5, r: &v };
    let b = &a; // taking a reference and as such explicitly sharing the value
    let x = a.x;
}
```
Now `b` explicitly points to `a`, so they are fully linked
## 2. Implementing core::copy::Copy (partial link)
We can implement the marker trait Copy to explicitly tell the compiler that a primitive copy is desired.

This is recommended when creating primitive datatypes like `Color` or an `Id` of some kind and is probably not appropiate here,
this is just for the sake of the example.
```rs
!use std::*;
use core::copy::Copy;
!
!struct Foo {
!    x: i32,
!    r: &i32
!}

impl Foo: Copy {}

fn main() {
    let v = 4;
    let a = Foo { x: 5, r: &v };
    let b = a; // gets copied like any other primitive value
    let x = a.x; // is now alled
}
```
Now `b` explicitly points to `a`, so they are fully linked

## 3. Implementing core::clone::Clone
This is recommended when you want to have two fully unlinked copies of a complex type, e.g. one with allocated fields like a vector.
```rs
!use std::*;
use std::mem::new;
use core::clone::Clone;
!
!struct Foo {
!    x: i32,
!    r: &i32
!}

impl Foo: Clone {
    fn clone(self: &Foo) -> Foo {
        _ { x: self.x, r: new(*self.r) }
    }
}

fn main() {
    let v = 4;
    let a = Foo { x: 5, r: &v };
    let b = a.clone(); // explicitly create unlinked instance
    let x = a.x; // is now alled
}
```

>**NOTE:** A type which is not Copy may also not be dereferenced, as it could be dereferenced multiple times, creating multiple semi linked instances