# Functions
Functions have zero or more argument types and an optional return type. Is the return type omitted, it is inferred to be [unit](./primitives/unit.md). Any other type is required to be explicit.
```rs
fn average(a: f32, b: f32) -> f32 {
    return (a + b)/ 2.0;
}
```
A block yields it's last value if the semicolon is omitted. As such this function achieves the same thing:
```rs
fn average(a: f32, b: f32) -> f32 {
    (a + b)/ 2.0
}
```
This is the preferred syntax. `return` is only used to return conditionally:
```rs
fn choose(condition: bool, a: i32, b: i32) -> f32 {
    if condition {
        return a;
    };
    b
}
```
However, this can be written more nicely:
```rs
fn choose(condition: bool, a: i32, b: i32) -> f32 {
    if condition {
        a
    } else {
        b
    }
}
```