# Pointers
Multiple pointer types exist and are each used in different circumstances

>**Note:**use `std::typecast<T, U>(t: T) -> U` to convert between a pointer type `T` and a pointer type `U`

- `ptr<T>` The most common pointer type
    - The unary `&` prefix operator turns a type `T` into a `ptr<T>` (read: pointer of type T)
    - The unary `*&*` prefix operator dereferences a `ptr<T>` into the pointed-to data of type T
```rs
!use std::*;
!
!fn main() {
    let int: i32 = 4;
    let int_ptr: ptr<i32> = &int;
    let int: i32 = *int_ptr;
!}
```
- `raw_ptr` an opaque ptr type. Used for c ffi
- `c_str` a pointer equal to `ptr<u8>` which points to a [string](./strings.md) literal.
>**Note:** If you want to access individual chars in the string, consider working with [slices](../../api/slice.md)
- `function_ptr<T>` Pointer to a function which returns a value of type `T`<br>
The function pointer needs to be wrapped in parenthesis to be called.
>**Note:** The types of the arguments are not conveyed in the type information and you are assumed to know the right type via api convention. As such function pointers should be wrapped in a usecase-specific safer api
```rs
!use std::*;
!use c_api::math::sin;
!
!fn main() {
    let sin_ptr: function_ptr<f32> = sin;
    let sin_of_one: f32 = (sin_ptr)(1.0);
!}
```