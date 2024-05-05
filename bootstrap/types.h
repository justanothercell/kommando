#ifndef TYPES_H
#define TYPES_H

#include "lib/defines.h"
#include "lib/list.h"
#include "ast.h"

#include <stdio.h>

typedef struct Struct {
    StrList fields;
    StrList fields_t;
    str name;
    usize src_line;
} Struct;

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
LIST(TypeList, Type*);

typedef struct TypeDef {
    str name;
    Type* type;
    StrList generics;
} TypeDef;
LIST(TypeDefList, TypeDef*);

typedef struct TypeValue TypeValue;
LIST(TypeValueList, TypeValue*);
typedef struct TypeValue {
    usize indirection;
    char* name;
    TypeValueList generics;
    usize src_line;
} TypeValue;


void drop_type_def(TypeDef* tydef);

void drop_type(Type* type);

void drop_struct(Struct* s);

usize field_offset(Module* module, Struct* s, str field, usize src_line);

Type* field_type(Module* module, Struct* s, str field, usize src_line);

void drop_type_value(TypeValue* value);

usize sizeof_primitive(PrimType pt);
usize sizeof_type(Module* module, Type* type, usize src_line);

void write_primitive(FILE* dest, PrimType pt);

void write_type(Module* module, FILE* dest, Type* ty);

void register_primitive(str type_name, PrimType t, TypeDefList* types);

void register_builtin_types(TypeDefList* types);

#endif