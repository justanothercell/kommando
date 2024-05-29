#ifndef TYPES_H
#define TYPES_H

#include "lib/defines.h"

#include "lib/list.h"

#include <stdio.h>

typedef struct Module Module;
typedef struct FunctionDef FunctionDef;
void write_code(FILE* dest, str code, ...);

typedef struct TypeValue TypeValue;
LIST(TypeValueList, TypeValue*);
typedef struct TypeValue {
    usize indirection;
    char* name;
    TypeValueList generics;
    usize src_line;
} TypeValue;

typedef struct Struct {
    StrList fields;
    TypeValueList fields_t;
    str name;
    usize src_line;
} Struct;

typedef enum PrimType {
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
    TYPE_STRUCT
} TypeT;

typedef struct Type {
    TypeT type;
    any ty;
} Type;
LIST(TypeList, Type*);

typedef struct GenericValue {
    str name;
    TypeValue* type;
} GenericValue;
LIST(GenericsMapping, GenericValue*);
LIST(GenericsMappingList, GenericsMapping);
typedef struct TypeDef {
    str name;
    Type* type;
    StrList generics;
    GenericsMappingList mappings;
} TypeDef;
LIST(TypeDefList, TypeDef*);

TypeValue* new_type_value(str name, usize src_line);
TypeValue* copy_type_value(TypeValue* type, usize src_line);

void drop_type_def(TypeDef* tydef);
void drop_generic_value(GenericValue* generic);
void drop_generic_mapping(GenericsMapping mapping);

void drop_type(Type* type);

void drop_struct(Struct* s);

usize field_offset(Module* module, Struct* s, str field, usize src_line);

Type* field_type(Module* module, Struct* s, str field);

void drop_type_value(TypeValue* value);

usize sizeof_primitive(PrimType pt);
usize sizeof_type(Module* module, Type* type, usize src_line);

void write_primitive(FILE* dest, PrimType pt);

void write_type(Module* module, FILE* dest, TypeDef* ty, usize indirection, TypeValueList* generics);
void write_func(Module* module, FILE* dest, FunctionDef* func, GenericsMapping* mapping) ;

void register_primitive(str type_name, PrimType t, TypeDefList* types);

void register_builtin_types(TypeDefList* types);

void fprint_func(FILE* file, FunctionDef* func);
typedef struct FunctionCall FunctionCall;
void fprint_func_value(FILE* file, FunctionCall* func);
void fprint_type(FILE* file, TypeDef* type);
void fprint_type_value(FILE* file, TypeValue* value);

#endif