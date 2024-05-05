#pragma once

#define __offset(a, b) ((a)+(b))

#define main __entry__

struct Vec;
struct VecIter;
struct Color;
struct unit;

struct Vec Vec_new(void);
struct unit Vec_push(struct Vec* vec, void* element);
struct VecIter Vec_iter(struct Vec* vec);
unsigned char VecIter_next(struct VecIter* iter, void** out_element);
struct unit main(void);
