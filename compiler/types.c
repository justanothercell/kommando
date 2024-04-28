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
    char* name;
    usize fields_c;
} Struct;

void drop_struct(Struct* s) {
    for (usize i = 0;i < s->fields_c;i++) {
        free(s->fields[i]);
        free(s->fields_t[i]);
    }
    free(s->fields);
    free(s->fields_t);
    free(s->name);
    free(s);
}

usize field_offset(Module* module, Struct* s, char* field) {
    usize offset = 0;
    for (usize i = 0;i < s->fields_c;i++) {
        if (strcmp(field, s->fields[i]) == 0) return true;
        offset += sizeof_type(module, find_type(module, s->fields_t[i]));
    }
    return offset;
}

Type* field_type(Module* module, Struct* s, char* field) {
    for (usize i = 0;i < s->fields_c;i++) {
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
            for (usize i = 0;i < s->fields_c;i++) {
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
        case TU8:       write_code(dest, "unsigned char"); return;
        case TU16:      write_code(dest, "unsigned short int"); return;
        case TU32:      write_code(dest, "unsigned long int"); return;
        case TU64:      write_code(dest, "unsigned long long int"); return;
        case TI8:       write_code(dest, "signed char"); return;
        case TI16:      write_code(dest, "signed short int"); return;
        case TI32:      write_code(dest, "signed long int"); return;
        case TI64:      write_code(dest, "signed long long int"); return;
        case TF32:      write_code(dest, "float"); return;
        case TF64:      write_code(dest, "long"); return;
        case TBool:     write_code(dest, "u8"); return;
    #ifdef __x86_64__
        case TUsize:    write_code(dest, "unsigned long long in"); return;
        case TIsize:    write_code(dest, "signed long long int"); return;
    #else
        case TUsize:    write_code(dest, "unsigned long in"); return;
        case TIsize:    write_code(dest, "signed long int"); return;
    #endif
    }
}

void write_type(Module* module, FILE* dest, Type* ty) {
    if (ty->type == TYPE_POINTER) {
        write_type(module, dest, (Type*)ty->ty);
        write_code(dest, "*");
    } else if (ty->type == TYPE_STRUCT) {
        write_code(dest, "struct %s", ((Struct*)ty->ty)->name);
    } else if (ty->type == TYPE_PRIMITIVE) {
        write_primitive(dest, *(PrimType*)ty->ty);
    }
}

void register_primitive(char* type_name, PrimType t, TypeDef*** types, usize* type_c, usize* capacity) {
    PrimType* p = malloc(sizeof(PrimType));
    *p = t;
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_PRIMITIVE;
    type->ty = p;
    TypeDef* tdef = malloc(sizeof(TypeDef));
    tdef->name = type_name;
    tdef->type = type;
    list_append(tdef, *types, *type_c, *capacity);
}

void register_builtin_types(TypeDef*** types, usize* type_c, usize* capacity) {
    Struct* s = malloc(sizeof(Struct));
    s->name = "unit";
    s->fields = NULL;
    s->fields_t = NULL;
    s->fields_c = 0;
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_STRUCT;
    type->ty = s;
    TypeDef* tdef = malloc(sizeof(TypeDef));
    tdef->name = "unit";
    tdef->type = type;
    list_append(tdef, *types, *type_c, *capacity);

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