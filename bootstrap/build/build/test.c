#include <stdio.h>
#include <stdlib.h>

#include "test.h"

#define main __entry__

struct Vec {
    unsigned long long int capacity;
    unsigned long long int length;
    void** elements;
};
struct VecIter {
    struct Vec* vec;
    unsigned long long int index;
};
struct Color {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};
struct unit {};

struct Vec Vec_new(void) {
    struct Vec vec;
    vec.capacity = 0;
    vec.length = 0;
    vec.elements = NULL;
    return vec;
}

struct unit Vec_push(struct Vec* vec, void* element) {
    if (((*vec).length >= (*vec).capacity)) {
        (*vec).capacity = (((*vec).capacity + 1) * 2);
        (*vec).elements = realloc((*vec).elements, ((*vec).capacity * 8));
    };
    (*__offset((*vec).elements, (*vec).length)) = element;
    (*vec).length = ((*vec).length + 1);
    return (struct unit) { };
}

struct VecIter Vec_iter(struct Vec* vec) {
    struct VecIter iter;
    iter.vec = vec;
    iter.index = 0;
    return iter;
}

unsigned char VecIter_next(struct VecIter* iter, void** out_element) {
    struct Vec* vec = (*iter).vec;
    if (((*vec).length > (*iter).index)) {
        (*out_element) = __offset((*vec).elements, (*iter).index);
        (*iter).index = ((*iter).index + 1);
        return 1;
    };
    (*out_element) = NULL;
    return 0;
}

struct unit main(void) {
    struct Color color;
    printf("color ptr: %p\n", (&color));
    struct Color* cref = (&color);
    printf("color ref: %p\n", cref);
    color.red = 255;
    color.green = 50;
    color.blue = 200;
    printf("blue: %u\n", color.blue);
    printf("blue from ref: %u\n", (*cref).blue);
    struct Vec vec = Vec_new();
    unsigned char i = 1;
    while ((i != 0)) {
        unsigned char* e = malloc(1);
        (*e) = i;
        Vec_push((&vec), e);
        i = (i * 2);
    };
    struct VecIter iter = Vec_iter((&vec));
    unsigned char* element = NULL;
    while (VecIter_next((&iter), (&element))) {
        printf("iter: %u\n", (*element));
    };
    return (struct unit) { };
}



#undef main

int main(int arg_c, char* arg_v[]) {
    __entry__();
    return 0;
}

