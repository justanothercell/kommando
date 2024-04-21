| type                | syntax                          | size (disregarding alignment and optimization) |
|---------------------|---------------------------------|-------------------------------------------------|
| named struct        | `struct { a: Type1, b: Type2 }` | `sizeof(Type1) + sizeof(Type2)`                 |
| tuple struct        | `(Type1, Type2, Type3)`         | `sizeof(Type1) + sizeof(Type2) + sizeof(Type3)` |
| enum                | `enum { A(Type1, Type2), B }`   | `sizeof(u8) + max(sizeof(A), sizeof(B))`        |
| union               | `union { a: Type1, b: Type2 }`  | `max(sizeof(Type1), sizeof(Type2))`             |
| unit                | `()`                            | `0`                                             |
| never               | `!`                             | `0`                                             |
| pointer             | `&Type`                         | `sizeof(usize)` (ptr)                           |
| trait               | `dyn Trait`                     | unsized                                         |
| trait pointer       | `dyn Trait`                     | `sizeof(usize) * 2` (ptr, vtable)               |
| array               | `[Type;length]`                 | `sizeof(Type) * length`                         |
| slice               | `[Type]`                        | unsized                                         |
| slice pointer       | `&[Type]`                       | `sizeof(usize) * 2` (ptr, size)                 |
| trait array         | `dyn [Trait;length]`            | unsized                                         |
| trait array pointer | `&dyn [Trait;length]`           | `sizeof(usize) * 2` (ptr, vtable);              |
| trait slice         | `dyn [Trait]`                   | unsized                                         |
| trait slice pointer | `&dyn [Trait]`                  | `sizeof(usize) * 3` (ptr, length, vtable);      |

| type                | syntax                          | size (disregarding alignment and optimization) |
|---------------------|---------------------------------|-------------------------------------------------|
| named struct        | `{ a: Type1, b: Type2 }`        | `sizeof(Type1) + sizeof(Type2)`                 |
| tuple struct        | `(Type1, Type2, Type3)`         | `sizeof(Type1) + sizeof(Type2) + sizeof(Type3)` |
| enum                | `[ A(Type1, Type2), B ]`        | `sizeof(u8) + max(sizeof(A), sizeof(B))`        |
| union               | `< a: Type1, b: Type2 >`        | `max(sizeof(Type1), sizeof(Type2))`             |
| unit                | `()`                            | `0`                                             |
| never               | `!`                             | `0`                                             |
| pointer             | `&Type`                         | `sizeof(usize)` (ptr)                           |
| trait               | `dyn Trait`                     | unsized                                         |
| trait pointer       | `dyn Trait`                     | `sizeof(usize) * 2` (ptr, vtable)               |
| array               | `[Type;length]`                 | `sizeof(Type) * length`                         |
| slice               | `[Type]`                        | unsized                                         |
| slice pointer       | `&[Type]`                       | `sizeof(usize) * 2` (ptr, size)                 |
| trait array         | `dyn [Trait;length]`            | unsized                                         |
| trait array pointer | `&dyn [Trait;length]`           | `sizeof(usize) * 2` (ptr, vtable);              |
| trait slice         | `dyn [Trait]`                   | unsized                                         |
| trait slice pointer | `&dyn [Trait]`                  | `sizeof(usize) * 3` (ptr, length, vtable);      |