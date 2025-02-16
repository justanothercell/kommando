# Conditionals
The simplest form of a condition is an if expression with a body:
```rs
if <cond> {
    <then>
};
```
>**Note:** Unlike in other languages, blocks need to be terminated by semicolons aswell if they are to be treated as a statement.

We can also specify what should be executed otherwise:
```rs
if <cond> {
    <then>
} else {
    <otherwise>
};
```
If expressions can also be chaned:
```rs
if <cond> {
    <then>
} else if <cond2> {
    <then2>
} else {
    <otherwise>
};
```
Values can be yielded from conditional blocks, same as regular blocks:
```rs
let num = 17;
let r = if num % 2 == 0 { 
    num / 2
} else {
    (num + 1) / 2
};
```
>**Note:** The yielded type from all branches must be matching