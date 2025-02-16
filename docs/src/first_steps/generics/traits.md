# Traits
Unlike with concrete types, we do not have a lot information to work with for our `T`.

This is where traits come to shine! Let's say we want to print some information about a box.
We just need to tell the compiler that `T` should itself implement `Fmt`, by introducing a so-called _trait bound_.

This ensures a contract that `T` is guaranteed to implement certain methods, in this case `Fmt::fmt` and `Fmt::dyn_fmt`:

```rs
!use std::*;
!use std::fmt::Fmt;
!use core::intrinsics::short_typename;
!
!struct Box<T> {
!    item: T
!}
!
!impl<T> Box<T> {
!    fn wrap(item: T) -> Box<T>  {
!        _ { item: item }
!    }
!
!    fn unwrap(self: Box<T>) -> T {
!        self.item
!    }
!}
!
impl<T: Fmt> Box<T> {
    fn print_info(self: ptr<Box<T>>) {
        println("Box contains $ [$]", short_typename::<T>().dyn_fmt(), self.item.dyn_fmt());
    }
}

fn main() {
    let i32b = Box::<i32>::wrap(4);
    let bb = Box::<_>::wrap(false);
    // we still need wildcard to disambiguate 
    let strb = Box::<_>::wrap("hello");

    i32b.print_info();
    bb.print_info();
    strb.print_info();
}
``` 