# Unit
The `unit` type is a zero sized type and the implicit return type for [functions](../functions.md) without an explicit return type.

```rs
fn main() -> unit { // implicit unit type as return
    unit { } // unit implicitly returned
}
```

>**Note:** unlike void in languages like C or Java, it can be assigned to, passed around and used in generics.

Functionally, it is no different than a [struct](../structs.md) with no fields, and that is in fact how it is defined.

>**Note:** Use `unit` in ffi where void should be returned. As the type is zero sized, the type is not unititialized if created in such a way.