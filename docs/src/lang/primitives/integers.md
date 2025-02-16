# Ints
An \\( n \\) bit integer is stored in memory with \\( n \\) bits or \\( \frac{n}{8} \\) bytes.

An integer literal is assumed to be of type `i32` but can be suffixed by an integer type to override this behavior: `4u8`

>**Note:** Use `std::numcast::<T, U>(t: T) -> U` to convert between numeric types (both float and int)

## Endianess
Depending on platfom these can be represented in memory in-order, with the highest-order byte first (big endian), or in reverse order, with the highest order byte last (little endian). This is only important when interpreting raw data as numbers or similar operations and does not cause issues when using bitwise ops.
>**Note:** Most modern systems such as Unix or Windows use little endian these days. It is highly unlikely that you will encounter big endian systems in the wild, as these are often highly specialized or deprecated technologies, like Mips or PowerPc.
The only time this may become important is when dealing with networking, as network numbers like ip adresses are represented as big endian.

## Signed
Fixed size signed integers take the form \\( \text{i}n \\) and have a range from \\( -2^{n-1} \\) to \\( 2^{n-1}-1 \\) and are stored in memory with \\( n \\) bits or \\( \frac{n}{8} \\) bytes.

Signed ints use the [two's complement](https://en.wikipedia.org/wiki/Two%27s_complement) representation
- `usize`
This type has the size of a pointer and is used to index array-like structures.
- `i8`
- `i16`
- `i32`
>**Note:** This is the type most commonly found in c ffi and also the default type for a number literal. YOu should use i32 in most cases dhat do not require a specific dimension.
- `i64`
- `i128`

## Unsigned
Fixed size unsigned integers take the form \\( \text{u}n \\) and have a range from \\( 0 \\) to \\( 2^n-1 \\) and are stored in memory with \\( n \\) bits or \\( \frac{n}{8} \\) bytes
- `isize`
This type has the same size as `usize` and can be used to compute index deltas.
- `u8`
>**Note:** `char` is an alias to u8 and can be used interchanably
- `u16`
- `u32`
- `u64`
- `u128`