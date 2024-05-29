#define __index(a, b) (*((a)+(b)))

#define main __entry__

struct Foo__u8__ {
    unsigned char value;
};
struct Foo__i8__ {
    signed char value;
};
struct Foo__REF_usize__ {
    unsigned long long int value;
};
struct Foo__REF_REF_usize__ {
    unsigned long long int value;
};
struct Foo__Foo____REF_usize______ {
    struct Foo__REF_usize__ value;
};
struct Foo__f32__ {
    float value;
};
struct Foo__REF_f32__ {
    float value;
};
struct Foo__Foo____f32______ {
    struct Foo value;
};
struct Foo__f64__ {
    long value;
};
struct Foo__REF_f64__ {
    long value;
};
struct Foo__Foo____f64______ {
    struct Foo value;
};
struct Foo__bool__ {
    unsigned char value;
};
struct Thing1 {
    struct Foo__bool__ foo;
};
struct Thing2 {
    struct Foo__bool__ foo;
};
struct Wrapper__REF_usize__ {
    struct Foo__REF_usize__ foo1;
    struct Foo__REF_REF_usize__ foo2;
    struct Foo__Foo____REF_usize______ foo3;
};
struct Wrapper__f32__ {
    struct Foo__f32__ foo1;
    struct Foo__REF_f32__ foo2;
    struct Foo__Foo____f32______ foo3;
};
struct Wrapper__f64__ {
    struct Foo__f64__ foo1;
    struct Foo__REF_f64__ foo2;
    struct Foo__Foo____f64______ foo3;
};
struct unit {};

struct Foo__u8__ new__Foo__u8__(void);
struct Foo__i8__ new__Foo__i8__(void);
struct Foo__REF_usize__ new__Foo__REF_usize__(void);
struct Wrapper__REF_usize__ new__Wrapper__REF_usize__(void);
struct Wrapper__f32__ new__Wrapper__f32__(void);
struct Wrapper__f64__ new__Wrapper__f64__(void);
struct Thing1 new__Thing1(void);
struct Thing2 new__Thing2(void);
struct unit main(void);
