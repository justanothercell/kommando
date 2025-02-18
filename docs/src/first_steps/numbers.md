# Numbers | Docs [[ints](../lang/primitives/integers.md), [floats](../lang/primitives/floats.md), [bools](../lang/primitives/bool.md)]
There are many different numeric types, but usually small subset suffices for most applications:
- `u8` or `char` (alias of the same type) when dealing with strings and raw byte sttreams
- `i32` for most standard numeric uses
- `usize` for counters, indicies and pointer arithmetic
- `f32` or `f64` depending on precision needs
- `u8`, `u32` or `u64` for bit masks or enumerations, depending on variant count
- `bool` for flags and conditions

You should try staying within a specific numeric type for a usecase, however sometimes you will need to convert between two different types.

For that use `std::numcast<T, U>(t: T) -> U` to do a C-style cast from a numeric type `T` to a numeric type `U`:

```rs
let a: u8 = 3;
let b: f32 = 4.2;
let r: f32 = numcast::<u8, f32>(a) * b;
```

This is equivalent to the following C code:

```c
unsigned char a = 3;
float b = 4.2;
float r = (float)a * b;
// r = 12.6
```

If you want to instead reinterpret the bytes of a type literally, use `std::typecast<T, U>(t: T) -> U`, which performs a pointer cast:

```rs
let f: f32 = 3.1415;
let f_bytes: u32 = typecast::<f32, u32>(f);
// f_bytes = 0b01000000010010010000111001010110 = 0x40490e56 = 1078529622
```

This is equivalent to the following C code:

```c
float f = 3.1415;
unsigned int f_bytes = *(unsigned int*)&f;
// f_bytes = 0b01000000010010010000111001010110 = 0x40490e56 = 1078529622
```

>**Note:** `typecast` only works for types of same size.