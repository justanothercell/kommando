# Floats
An \\( n \\) bit float is stored in memory with \\( n \\) bits or \\( \frac{n}{8} \\) bytes.
Floats conform to the [IEEE 754](https://en.wikipedia.org/wiki/IEEE_754) floating poitn standard.

A float literal aliases to either `f32` or `f64` as required.

- `f16`
>**Note:** This type is only supported as a storage type and will be interpreted as a `f32` during math operations.
This conversion might make things slower and as such the usage of this type is discouraged unless memory is of concern.
- `f32`
- `f64`
- `f128`
>**Note:** Due to lack of hardware support this type might be very slow to use. Consider employing different technologies and reflect whether such a precicion is really required.