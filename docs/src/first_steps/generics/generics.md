# Generics

Sometimes structures and functions are supposed to work for multiple datataypes:
```rs
!use std::*;
!
struct I32Container {
    item: i32
}
struct BoolContainer {
    item: bool
}
// and so on...

fn main() {
    let i32b = I32Container { item: 4 };
    let bb = BoolContainer { item: false };
}
```
As an alternative to creating a variant for each possible type, we can replace the concrete `item` type with a _generic_ placeholder type `T`:
```rs
!use std::*;
!
struct Container<T> {
    item: T
}

fn main() {
    let i32b = Container::<i32> { item: 4 };
    // `_` wildcard is inferred to be bool
    let bb = Container::<_> { item: false };
    // omitted type is inferred to be c_str
    let strb = Container { item: "hello" };
}
```
We can also make methods generic to work on any `T`:
```rs
!use std::*;
!
!struct Container<T> {
!    item: T
!}
!
// "for any T we want a Container of T with the follwing methods"
impl<T> Container<T> {
    fn wrap(item: T) -> Container<T>  {
        // wildcard inferred to be Container::<T>
        _ { item: item }
    }

    // `self` name is arbitrary
    fn unwrap(self: Container<T>) -> T {
        self.item
    }
}

fn main() {
    let i32b = Box::<i32>::wrap(4);
    let bb = Box::<_>::wrap(false);
    // we still need wildcard to disambiguate 
    let strb = Box::<_>::wrap("hello");

    let i = i32b.unwrap();
    let b = bb.unwrap();
    let s = strb.unwrap();
}
```
>**Note:** Technically we already know one generic: `ptr<T>`
