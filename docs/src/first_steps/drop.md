# Drop
Some resources require cleanup after use, e.g. freeing of heap allocations or closing of file handles.

```rs
!use std::*;
!use std::mem::new;
!use std::mem::free;
!use c_api::printf;
!
fn main() {
    let x: &i32 = new(1); // allocting memory
    printf("*%p = %d\n", x, *x);
    *x = 2;
    printf("*%p = %d\n", x, *x);
    free(x); // freeing memory
}
```

However, this can easily be forgotten and result in a resource leak, or otherwise cause problems later when someone tries to access the same resource again.

The `core::drop::Drop` trait, mutually exclusive with `core::copy::Copy`, is run recursively for a struct and each of its fields once it goes out of scope.

```rs
!use std::*;
!use c_api::printf;
use core::drop::Drop;

struct Foo {}

impl Foo: Drop {
    fn drop(self: &Foo) {
        printf("Dropped Foo");
    }
}

fn main() {
    let f = Foo {}
} // f is dropped!
```

This even automatically works when f is a field in a struct:
```rs
!use std::*;
!use c_api::printf;
!use core::drop::Drop;
!
!struct Foo {}
!
!impl Foo: Drop {
!    fn drop(self: &Foo) {
!        printf("Dropped Foo");
!    }
!}
!
struct Wrapper {
    f: Foo
}

fn main() {
    let w = Wrapper { f: Foo {} };
} // w is dropped (trivial) and w.f is also dropped!
```

>**Note:** This does not take lifetimes of references into account and using a reference after the value goes out of scope is undefined behavior, using the same rules as discussed in the chapter about [pointers](./pointers.md). This holds true for _all_ pointers and is not a drop specific behavior.

```rs
!use std::*;
!use c_api::printf;
!use core::drop::Drop;
!
!struct Foo {}
!
!impl Foo: Drop {
!    fn drop(self: &Foo) {
!        printf("Dropped Foo");
!    }
!}
!
fn main() {
    let invalid_ref = {
        let f = Foo {};
        let r = &f;
        r
    }; // f is dropped but the reference survives
}
```

Now back to our allocation example, we will build a type which frees allocated memory after it goes out of scope

We call this type a `Box`
```rs
!use std::*;
!
!use core::clone::Clone;
!use core::drop::Drop;
!use std::mem::new;
!
pub struct Box<T> {
    item: &T
}

impl<T> Box<T> {
    pub fn new(t: T) -> Box<T> {
        _ { item: new(t) }
    }
    pub fn ref(self: &Box<T>) -> &T {
        self.item
    }
}
```

Now we implement the drop trait and try it out:

```rs
!use std::*;
!
!use core::clone::Clone;
!use core::drop::Drop;
!use core::intrinsics::short_typename;
!use std::mem::new;
!use std::mem::free;
!use c_api::printf;
!
!pub struct Box<T> {
!    item: &T
!}
!
!impl<T> Box<T> {
!    pub fn new(t: T) -> Box<T> {
!        _ { item: new(t) }
!    }
!    pub fn ref(self: &Box<T>) -> &T {
!        self.item
!    }
!}
!
impl<T> Box<T>: Drop {
    pub fn drop(self: &Box<T>) {
        printf("%s freed\n", short_typename::<Box<T>>());
        free(self.item);
    }
}

fn main() {
    let b = Box::<_>::new(42); // allocated
    *b.ref() = 7;
    printf("%d\n", *b.ref());
} // destructor is run and the allocation is freed
```
Outout:
```
7
Box2<i32> freed
```

However, we are not quite done yet, as can be seen by the following scenario:
```rs
!use std::*;
!
!use core::clone::Clone;
!use core::drop::Drop;
!use core::intrinsics::short_typename;
!use std::mem::new;
!use std::mem::free;
!use c_api::printf;
!
!pub struct Box<T> {
!    item: &T
!}
!
!impl<T> Box<T> {
!    pub fn new(t: T) -> Box<T> {
!        _ { item: new(t) }
!    }
!    pub fn ref(self: &Box<T>) -> &T {
!        self.item
!    }
!}
!
!impl<T> Box<T>: Drop {
!    pub fn drop(self: &Box<T>) {
!        printf("%s freed\n", short_typename::<Box<T>>());
!        free(self.item);
!    }
!}
!
fn main() {
    let b = Box::<_>::new(42);
    let bb = Box::<_>::new(b);
}
```
Output:
```
Box<Box<i32>> freed
```
The inner box is not dropped!

As stated previously, all fields are recursively dropped for any noncopy struct, without any exra effort needed.

This however, is a special case since the type of `item` is `&T` and not `T, therefore only the reference is dropped and not the underlying value itself, requiring us to do a bit of manual work:

```rs
!use std::*;
!
!use core::clone::Clone;
!use core::drop::Drop;
!use core::intrinsics::short_typename;
!use std::mem::new;
!use std::mem::free;
!use std::mem::take_unchecked;
!use c_api::printf;
!
!pub struct Box<T> {
!    item: &T
!}
!
!impl<T> Box<T> {
!    pub fn new(t: T) -> Box<T> {
!        _ { item: new(t) }
!    }
!    pub fn ref(self: &Box<T>) -> &T {
!        self.item
!    }
!}
!
impl<T> Box<T>: Drop {
    pub fn drop(self: &Box<T>) {
        take_unchecked(self.item); // takes the value of the item reference and returns it, so that it can be dropped immediately
        printf("%s freed\n", short_typename::<Box<T>>());
        free(self.item);
    }
}
!
!fn main() {
!    let b = Box::<_>::new(42);
!    let bb = Box::<_>::new(b);
!}
```
Now the Box finally works:
```rs
Box<i32> freed
Box<Box<i32>> freed
```

**Note:** This implementation can be found at `std::mem::box::Box`