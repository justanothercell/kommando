# Expressions
Special care has been taken than anything can be used inside an expression. While the following would not work in languages like C
```rs
fn main() {
    let x = return;
}
```
this is perfectly valid kommando syntax and `x` (although unreachable) would get the type `unit`.

An expression followed by a semicolon is called a statement. Every expression must be a statement, except optionally for the very last one in a block:
```rs
fn xor(a: u8, b: u8) -> u8 {
    a ^ b
}
```
is equivalent to:
```rs
fn xor(a: u8, b: u8) -> u8 {
    return a ^ b;
}
```
This allows you to yield values from arbitrary blocks:
```rs
let r = { // r is now 25
    let a = 5;
    a * a
};
```