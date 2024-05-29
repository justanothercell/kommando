#include <stdio.h>
#include <stdlib.h>

#include "simple.h"

#define main __entry__


struct Foo__u8__ new__Foo__u8__(void) {
    struct Foo__u8__ value;
    return value;
}

struct Foo__i8__ new__Foo__i8__(void) {
    struct Foo__i8__ value;
    return value;
}

struct Foo__REF_usize__ new__Foo__REF_usize__(void) {
    struct Foo__REF_usize__ value;
    return value;
}

struct Wrapper__REF_usize__ new__Wrapper__REF_usize__(void) {
    struct Wrapper__REF_usize__ value;
    return value;
}

struct Wrapper__f32__ new__Wrapper__f32__(void) {
    struct Wrapper__f32__ value;
    return value;
}

struct Wrapper__f64__ new__Wrapper__f64__(void) {
    struct Wrapper__f64__ value;
    return value;
}

struct Thing1 new__Thing1(void) {
    struct Thing1 value;
    return value;
}

struct Thing2 new__Thing2(void) {
    struct Thing2 value;
    return value;
}

struct unit main(void) {
    struct Foo__u8__ f1 = new();
    struct Foo__i8__ f2 = new();
    struct Foo__REF_usize__ f3 = new();
    struct Wrapper__REF_usize__ f4 = new();
    struct Wrapper__f32__ f5 = new();
    struct Wrapper__f64__ f6 = new();
    struct Thing1 f7 = new();
    struct Thing2 f8 = new();
    return (struct unit) { };
}



#undef main

int main(int arg_c, char* arg_v[]) {
    __entry__();
    return 0;
}

