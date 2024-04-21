#pragma once

#include "lib/defines.c"
#include "lib/list.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;
typedef struct Module Module;
void write_code(FILE* dest, const char* format, ...);
void drop_type(Type* type);
usize sizeof_type(Module* module, Type* type);
Type* find_type(Module* module, char* type);

typedef struct Struct {
    char** fields;
    char** fields_t;
    usize field_c;
} Struct;

void drop_struct(Struct* s) {
    for (usize i = 0;i < s->field_c;i++) {
        free(s->fields[i]);
        free(s->fields_t[i]);
    }
    free(s->fields);
    free(s->fields_t);
    free(s);
}

usize field_offset(Module* module, Struct* s, char* field) {
    usize offset = 0;
    for (usize i = 0;i < s->field_c;i++) {
        if (strcmp(field, s->fields[i]) == 0) return true;
        offset += sizeof_type(module, find_type(module, s->fields_t[i]));
    }
    return offset;
}

Type* field_type(Module* module, Struct* s, char* field) {
    for (usize i = 0;i < s->field_c;i++) {
        if (strcmp(field, s->fields[i]) == 0) {
            return find_type(module, s->fields_t[i]);
        }
    }
    return false;
}

typedef enum PrimType {
    TU8,
    TU16,
    TU32,
    TU64,
    TI8,
    TI16,
    TI32,
    TI64,
    TF32,
    TF64,
    TBool,
    TUsize,
    TIsize
} PrimType;

typedef enum TypeT {
    TYPE_PRIMITIVE,
    TYPE_STRUCT,
    TYPE_POINTER
} TypeT;

typedef struct Type {
    TypeT type;
    void* ty;
} Type;

void drop_type(Type* type) {
    switch (type->type) {
        case TYPE_PRIMITIVE:
            free(type->ty);
            break;
        case TYPE_STRUCT:
            drop_struct((Struct*)type->ty);
            break;
        case TYPE_POINTER:
            drop_type((Type*)type->ty);
            break;
    }
    free(type);
}

typedef struct TypeDef {
    char* name;
    Type* type;
} TypeDef;

void drop_type_def(TypeDef* tydef) {
    free(tydef->name);
    drop_type(tydef->type);
    free(tydef);
}

usize sizeof_primitive(PrimType pt) {
    switch (pt) {
        case TU8:       return sizeof(u8);
        case TU16:      return sizeof(u16);
        case TU32:      return sizeof(u32);
        case TU64:      return sizeof(u64);
        case TI8:       return sizeof(i8);
        case TI16:      return sizeof(i16);
        case TI32:      return sizeof(i32);
        case TI64:      return sizeof(i64);
        case TF32:      return sizeof(f32);
        case TF64:      return sizeof(f64);
        case TBool:     return sizeof(bool);
        case TUsize:    return sizeof(usize);
        case TIsize:    return sizeof(isize);
    }
    return 0;
}

usize sizeof_type(Module* module, Type* type) {
    switch (type->type) {
        case TYPE_PRIMITIVE:
            return sizeof_primitive(*(PrimType*)type->ty);
        case TYPE_STRUCT: 
        {
            usize size = 0;
            Struct* s = type->ty;
            for (usize i = 0;i < s->field_c;i++) {
                usize f_size = 0;
                Type* f_type = NULL;
                size += sizeof_type(module, find_type(module, s->fields_t[i]));
            }
            return true;
        }
        case TYPE_POINTER:
            return sizeof(void*);
    }
    return 0;
}

void write_primitive(FILE* dest, PrimType pt) {
    switch (pt) {
        case TU8:       write_code(dest, "u8"); return;
        case TU16:      write_code(dest, "u16"); return;
        case TU32:      write_code(dest, "u32"); return;
        case TU64:      write_code(dest, "u64"); return;
        case TI8:       write_code(dest, "i8"); return;
        case TI16:      write_code(dest, "i16"); return;
        case TI32:      write_code(dest, "i32"); return;
        case TI64:      write_code(dest, "i64"); return;
        case TF32:      write_code(dest, "f32"); return;
        case TF64:      write_code(dest, "f64"); return;
        case TBool:     write_code(dest, "bool"); return;
        case TUsize:    write_code(dest, "usize"); return;
        case TIsize:    write_code(dest, "isize"); return;
    }
}

void write_type(Module* module, FILE* dest, Type* type) {
    switch (type->type) {
        case TYPE_PRIMITIVE:
            write_primitive(dest, *(PrimType*)type->ty);
            return;
        case TYPE_STRUCT:
            write_code(dest, "struct { u8 value[%lld] }", sizeof_type(module, type));
            return;
        case TYPE_POINTER:
            write_code(dest, "struct { u8 value[%lld] }*", sizeof_type(module, type));
            return;
    }
}

void write_field_access(Module* module, FILE* dest, Struct* s, char* name, char* field, bool ptr) {
    write_code(dest, "(");
    write_type(module, dest, field_type(module, s, field));
    write_code(dest, "*)(%s", field);
    if (ptr) write_code(dest, "->");
    else write_code(dest, ".");
    write_code(dest, "value+%lld)", field_offset(module, s, field));
}

void register_primitive(char* type_name, PrimType t, TypeDef*** types, usize* type_c, usize* capacity) {
    printf("before %lld %lld\n", *type_c, *capacity);
    PrimType* p = malloc(sizeof(PrimType));
    *p = t;
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_PRIMITIVE;
    type->ty = p;
    TypeDef* tdef = malloc(sizeof(TypeDef));
    tdef->name = type_name;
    tdef->type = type;
    list_append(tdef, *types, *type_c, *capacity); 
    printf("after %lld %lld\n", *type_c, *capacity);
}

void register_primitives(TypeDef*** types, usize* type_c, usize* capacity) {
    register_primitive("u8", TU8, types, type_c, capacity);
    register_primitive("u16", TU16, types, type_c, capacity);
    register_primitive("u32", TU32, types, type_c, capacity);
    register_primitive("u64", TU64, types, type_c, capacity);
    register_primitive("i8", TI8, types, type_c, capacity);
    register_primitive("i16", TI16, types, type_c, capacity);
    register_primitive("i32", TI32, types, type_c, capacity);
    register_primitive("i64", TI64, types, type_c, capacity);
    register_primitive("f32", TF32, types, type_c, capacity);
    register_primitive("f64", TF64, types, type_c, capacity);
    register_primitive("bool", TBool, types, type_c, capacity);
    register_primitive("usize", TUsize, types, type_c, capacity);
    register_primitive("isize", TIsize, types, type_c, capacity);
}