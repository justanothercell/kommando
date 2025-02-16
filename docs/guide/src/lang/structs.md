# Structs
```rs
!use std::*;
!use std::core::intrinsics::numcast;
!
struct Car {
    wheels: u32,
    max_speed: f32
}

fn main() {
    // creating struct using struct literal syntax
    let car = Car { wheels: 4, max_speed: 50.0 };
    // accessing fields of a struct
    let speed_per_wheel = car.max_speed / numcast(car.wheels);

    let car_ptr = &car;
    // A struct automatically dereferences when accessing a field for convenience
    let speed = car_ptr.max_speed;
    // Equivalent to 
    let speed = (*car_ptr).max_speed;
}
```
>**Note:** A struct is a product type in type theory: \\( |T| = \prod_{T_i \in T}{ |T_i| } \\)<br>where \\( |T| \\) is the number of possible states of a struct T
