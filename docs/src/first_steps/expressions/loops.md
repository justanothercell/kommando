# Loops
## While
```rs
while <cond> {
    <body>
};
```
Example:
```rs
!use std::*;
!
!fn main() {
let i = 0usize;
while i < 10 {
    c_api::printf("%d\n", i);
    i += 1;
};
!}
```
You can also break out of a loop:
```rs
!use std::*;
!
!fn main() {
let i = 0usize;
while i < 10 {
    c_api::printf("%d\n", i);
    if i >= 5 { 
        break;
    };
    i += 1;
};
!}
```
Or skip the rest of the loop body this iteration:
```rs
!use std::*;
!
!fn main() {
let i = 0usize;
while i < 10 {
    i += 1;
    if i >= 5 { 
        c_api::printf("skipping!\n");
        continue;
    };
    c_api::printf("%d\n", i);
};
!}
```
>**Note:** Same as `return`, `break` and `continue` yield a type of `unit` and any code after is unreachable
```rs
let r: unit = return;
let r: unit = break;
let r: unit = continue;
```