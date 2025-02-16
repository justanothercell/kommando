# Structs
```rs
!use std::*;
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
## Size and alignment

\\( \textrm{let } \\{ T_0, ..., T_n \\} = \mathbb{T} \textrm{ be a compound struct type, then} \\)

\\( 
\text{alignof}(\mathbb{T}) =
\left\\{ 
  \begin{array}{ c l }
    0 & \quad \textrm{if } \mathbb{T} = \emptyset \\\\
    \max_{T_i \in \mathbb{T}}{T_i} & \quad otherwise
  \end{array}
\right.
\\)

\\(
\text{offsetof}(T_i) =
\left\\{ 
  \begin{array}{ c l }
    0 & \quad \textrm{if } i = 0 \\\\
    \lceil \frac{\text{offsetof}(T_{i-1})+\text{sizeof}(T_{i-1})}{\text{alignof}(T_{i-1})} \rceil \text{alignof}(T_{i-1}) & \quad otherwise
  \end{array}
\right.
\\)

\\(
\text{sizeof}(\mathbb{T}) =
\left\\{ 
  \begin{array}{ c l }
    0 & \quad \textrm{if } \mathbb{T} = \emptyset \\\\
    \lceil \frac{\text{offsetof}(T_n)+\text{sizeof}(T_n)}{\text{alignof}(\mathbb{T})} \rceil \text{alignof}(\mathbb{T}) & \quad otherwise
  \end{array}
\right.
\\)

Size and alignment for primitives is hardcoded.

>**Note:** A struct is a product type in type theory: \\( |\mathbb{T}| = \prod_{T_i \in \mathbb{T}}{ |T_i| } \\)<br>where \\( |\mathbb{T}| \\) is the number of possible states of \\(\mathbb{T}\\).<br>This may not be confused with physical size, as foór example \\( |\text{bool}| = 2 \\)<br>but \\(\text{sizeof}(bool) = 1 \text{byte} = 8 \text{bit} \\). As such, \\( \text{sizeof}(\mathbb{T}) \ge |\mathbb{T}| \\)
