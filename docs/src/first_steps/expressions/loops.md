# Loops
## While
```rs
while <cond> {
    <body>
};
```
Example:
```rs
let i = 0usize;
while i < 100 {
    c_api::printf("%d\n", i);
    i += 1;
};
```