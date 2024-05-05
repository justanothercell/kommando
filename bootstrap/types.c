#pragma once

#include "lib/defines.c"
#include "lib/list.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;
LIST(TypeList, Type*);
typedef struct Module Module;
typedef struct FunctionDef FunctionDef;
LIST(FunctionDefList, FunctionDef*);
void write_code(FILE* dest, const str format, ...);
void drop_type(Type* type);
usize sizeof_type(Module* module, Type* type);
Type* find_type(Module* module, str type);

typedef struct Struct {
    StringList fields;
    StringList fields_t;
    str name;
} Struct;

void drop_struct(Struct* s) {
    list_foreach(&(s->fields), free);
    list_foreach(&(s->fields_t), free);
    free(s->fields.elements);
    free(s->fields_t.elements);
    free(s->name);
    free(s);
}

usize field_offset(Module* module, Struct* s, str field) {
    usize offset = 0;
    for (usize i = 0;i < s->fields.length;i++) {
        if (strcmp(field, s->fields.elements[i]) == 0) return true;
        offset += sizeof_type(module, find_type(module, s->fields_t.elements[i]));
    }
    return offset;
}

Type* field_type(Module* module, Struct* s, str field) {
    for (usize i = 0;i < s->fields.length;i++) {
        if (strcmp(field, s->fields.elements[i]) == 0) {
            return find_type(module, s->fields_t.elements[i]);
        }
    }
    return false;
}

typedef enum PrimType {
    TStr,
    TAny,
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
    any ty;
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
    str name;
    Type* type;
} TypeDef;
LIST(TypeDefList, TypeDef*);

void drop_type_def(TypeDef* tydef) {
    free(tydef->name);
    drop_type(tydef->type);
    free(tydef);
}

usize sizeof_primitive(PrimType pt) {
    switch (pt) {
        case TStr:      return sizeof(str);
        case TAny: fprintf(stderr, "`any` is unsized, use as reference instead: `&any`"); exit(1);
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
            for (usize i = 0;i < s->fields.length;i++) {
                usize f_size = 0;
                Type* f_type = NULL;
                size += sizeof_type(module, find_type(module, s->fields_t.elements[i]));
            }
            return true;
        }
        case TYPE_POINTER:
            return sizeof(any);
    }
    return 0;
}

void write_primitive(FILE* dest, PrimType pt) {
    switch (pt) {
        case TStr:      write_code(dest, "char*"); return;
        case TAny:      write_code(dest, "void"); return;
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
        case TBool:     write_code(dest, "unsigned char"); return;
    #ifdef __x86_64__
        case TUsize:    write_primitive(dest, TU64); return;
        case TIsize:    write_primitive(dest, TI64); return;
    #else
        case TUsize:    write_primitive(dest, TU32); return;
        case TIsize:    write_primitive(dest, TI32); return;
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

void register_primitive(str type_name, PrimType t, TypeDefList* types) {
    PrimType* p = malloc(sizeof(PrimType));
    *p = t;
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_PRIMITIVE;
    type->ty = p;
    TypeDef* tdef = malloc(sizeof(TypeDef));
    tdef->name = copy_str(type_name);
    tdef->type = type;
    list_append(types, tdef);
}

void register_builtin_types(TypeDefList* types) {
    Struct* s = malloc(sizeof(Struct));
    s->name = copy_str("unit");
    s->fields = (StringList)list_new();
    s->fields_t = (StringList)list_new();
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_STRUCT;
    type->ty = s;
    TypeDef* tdef = malloc(sizeof(TypeDef));
    tdef->name = copy_str("unit");
    tdef->type = type;
    list_append(types, tdef);

    register_primitive("str", TStr, types);
    register_primitive("any", TAny, types);
    register_primitive("u8", TU8, types);
    register_primitive("u16", TU16, types);
    register_primitive("u32", TU32, types);
    register_primitive("u64", TU64, types);
    register_primitive("i8", TI8, types);
    register_primitive("i16", TI16, types);
    register_primitive("i32", TI32, types);
    register_primitive("i64", TI64, types);
    register_primitive("f32", TF32, types);
    register_primitive("f64", TF64, types);
    register_primitive("bool", TBool, types);
    register_primitive("usize", TUsize, types);
    register_primitive("isize", TIsize, types);
}