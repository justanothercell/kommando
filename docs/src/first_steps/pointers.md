# Pointers

All objects are passed by value and as such copied. That means once you pass a value, you are disconnected from it.

```rs
!use std::*;
!
fn increment(v: u32) {
    v += 1;
    c_api::printf("incremented: %d", v);
}
fn main() {
    let value = 4;
    increment(value);
    c_api::printf("not incremented: %d", value);
}
```
Increment gets a copy of `value` and as such the original is not incremented.

This can however be done by explicitly taking a reference:
```rs
!use std::*;
!
fn increment(v: ptr<i32>) { // v points to value
    *v += 1; // increment the value pointed to by v
    c_api::printf("incremented: %d\n", *v); // read the value pointed to by v
}

fn main() {
    let value = 4;
    let points_to_value = &value; // point to value
    increment(points_to_value);
    c_api::printf("now also incremented: %d\n", value);
}
```
Here we _take a reference_ of `value` by using the `&` prefix operator.<br>
To access the inner value inside the pointer, we need to _dereference_ it using the `*` prefix operator.

A pointer is only valid as long as the original object is valid:
```rs
!use std::*;
!
fn create_pointer() -> ptr<i32> {
    let value = 0;
    let p = &value; // create pointer to value
    p // value still exists...
} // value gets removed at end of function, p is now invalid!

fn main() {
    // p is invalid here since value was already removed
    let p = create_pointer();
}
```
`p` may now have any and all states, or may even crash the program when used.
Even if the original object is still around but was moved, `p` is still invalid