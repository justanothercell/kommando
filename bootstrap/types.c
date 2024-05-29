#include "types.h"

#include "lib/defines.h"

#include "lib/list.h"
#include "lib/str.h"
#include "ast.h"
#include "infer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void drop_struct(Struct* s) {
    list_foreach(&(s->fields), free);
    list_foreach(&(s->fields_t), free);
    free(s->fields.elements);
    free(s->fields_t.elements);
    free(s->name);
    free(s);
}

usize field_offset(Module* module, Struct* s, str field, usize src_line) {
    usize offset = 0;
    for (usize i = 0;i < s->fields.length;i++) {
        if (strcmp(field, s->fields.elements[i]) == 0) return true;
        offset += sizeof_type(module, find_type_def(module, s->fields_t.elements[i])->type, s->fields_t.elements[i]->src_line);
    }
    return offset;
}

Type* field_type(Module* module, Struct* s, str field) {
    for (usize i = 0;i < s->fields.length;i++) {
        if (strcmp(field, s->fields.elements[i]) == 0) {
            return find_type_def(module, s->fields_t.elements[i])->type;
        }
    }
    return false;
}

void fprint_func(FILE* file, FunctionDef* func) {
    fprintf(file, "%s", func->name);
    if (func->generics.length > 0) {
        fprintf(file, "<");
        for (usize i = 0;i < func->generics.length;i++) {
            if (i > 0) fprintf(file, ", ");
            printf("%s", func->generics.elements[i]);
        }
        fprintf(file, ">");
    }
    printf("(");
    for (usize i = 0;i < func->args.length;i++) {
        if (i > 0) fprintf(file, ", ");
        printf("%s: ", func->args.elements[i]);
        fprint_type_value(file, func->args_t.elements[i]);    
    }
    if (func->is_variadic) {
        if (func->args.length > 0) fprintf(file, ", ");
        fprintf(file, "*");
    }
    printf(")");
}

void fprint_func_value(FILE* file, FunctionCall* func) {
    fprintf(file, "fn %s", func->name);
    if (func->generics.length > 0) {
        fprintf(file, "<");
        for (usize i = 0;i < func->generics.length;i++) {
            if (i > 0) fprintf(file, ", ");
            fprint_type_value(file, func->generics.elements[i]);
        }
        fprintf(file, ">");
    }
}

void fprint_type(FILE* file, TypeDef* type) {
    fprintf(file, "%s", type->name);
    if (type->generics.length > 0) {
        fprintf(file, "<");
        for (usize i = 0;i < type->generics.length;i++) {
            if (i > 0) fprintf(file, ", ");
            printf("%s", type->generics.elements[i]);
        }
        fprintf(file, ">");
    }
}

void fprint_type_value(FILE* file, TypeValue* value) {
    for (usize i = 0;i < value->indirection;i++) fprintf(file, "&");
    fprintf(file, "%s", value->name);
    if (value->generics.length > 0) {
        fprintf(file, "<");
        for (usize i = 0;i < value->generics.length;i++) {
            if (i > 0) fprintf(file, ", ");
            fprint_type_value(file, value->generics.elements[i]);
        }
        fprintf(file, ">");
    }
}

void drop_type(Type* type) {
    switch (type->type) {
        case TYPE_PRIMITIVE:
            free(type->ty);
            break;
        case TYPE_STRUCT:
            drop_struct((Struct*)type->ty);
            break;
    }
    free(type);
}

void drop_type_def(TypeDef* tydef) {
    free(tydef->name);
    drop_type(tydef->type);
    list_foreach(&(tydef->generics), free);
    free(tydef->generics.elements);
    list_foreach(&(tydef->mappings), drop_generic_mapping);
    free(tydef->mappings.elements);
    free(tydef);
    
}

void drop_type_value(TypeValue* value) {
    list_foreach(&(value->generics), drop_type_value);
    free(value->generics.elements);
    free(value->name);
    free(value);
}

void drop_generic_mapping(GenericsMapping mapping) {
    list_foreach(&mapping, drop_generic_value);
    free(mapping.elements);
}

void drop_generic_value(GenericValue* generic) {
    free(generic->name);
    // generic->type only borrowed!
    free(generic);
}

TypeValue* new_type_value(str name, usize src_line) {
    TypeValue* new = malloc(sizeof(TypeValue));
    new->name = name;
    new->generics = (TypeValueList)list_new();
    new->indirection = 0;
    new->src_line = src_line;
    return new;
}

TypeValue* copy_type_value(TypeValue* type, usize src_line) {
    TypeValue* new = malloc(sizeof(TypeValue));
    new->indirection = type->indirection;
    new->name = copy_str(type->name);
    new->generics = (TypeValueList)list_new();
    for (usize i = 0;i < type->generics.length;i++) { 
        list_append(&(new->generics), copy_type_value(type->generics.elements[i], src_line));
    }
    new->src_line = src_line;
    return new;
}

usize sizeof_primitive(PrimType pt) {
    switch (pt) {
        case TAny:      fprintf(stderr, "`any` is unsized, use as reference instead: `&any`"); exit(1);
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

usize sizeof_type(Module* module, Type* type, usize src_line) {
    switch (type->type) {
        case TYPE_PRIMITIVE:
            return sizeof_primitive(*(PrimType*)type->ty);
        case TYPE_STRUCT: 
        {
            usize size = 0;
            Struct* s = type->ty;
            for (usize i = 0;i < s->fields.length;i++) {
                size += sizeof_type(module, find_type_def(module, s->fields_t.elements[i])->type, src_line);
            }
            return true;
        }
    }
    return 0;
}

void write_primitive(FILE* dest, PrimType pt) {
    switch (pt) {
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

void write_primitive_name(FILE* dest, PrimType pt) {
    switch (pt) {
        case TAny:      write_code(dest, "any"); return;
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

void write_type_inner(Module* module, FILE* dest, TypeDef* ty, usize indirection, TypeValueList* generics, usize nesting) {
    for (usize i = 0;i < nesting;i++) write_code(dest, "__");
    for (usize i = 0;i < indirection;i++) write_code(dest, "REF_");
    if (ty->type->type == TYPE_STRUCT) {
        write_code(dest, "%s", ((Struct*)ty->type->ty)->name);
        if (generics != NULL) {
            for (usize i = 0;i < generics->length;i++) {
                if (i > 0 ) write_code(dest, "__");
                TypeValue* tv = degenerify(generics->elements[i], &ty->generics, generics, generics->elements[i]->src_line);
                write_type_inner(module, dest, find_type_def(module, tv), tv->indirection, &tv->generics, nesting + 1);
            }
        }
    } else if (ty->type->type == TYPE_PRIMITIVE) {
        if (generics != NULL && generics->length != 0) {
            fprintf(stderr, "Primitive should not have generics");
            exit(1);
        }
        write_primitive_name(dest, *(PrimType*)ty->type->ty);
    }
    for (usize i = 0;i < nesting;i++) write_code(dest, "__");
}
void write_type(Module* module, FILE* dest, TypeDef* ty, usize indirection, TypeValueList* generics) {
    if (ty->type->type == TYPE_STRUCT) {
        write_code(dest, "struct ");
        write_type_inner(module, dest, ty, 0, generics, 0);
    } else if (ty->type->type == TYPE_PRIMITIVE) {
        if (generics != NULL && generics->length != 0) {
            fprintf(stderr, "Primitive should not have generics");
            exit(1);
        }
        write_primitive(dest, *(PrimType*)ty->type->ty);
    }
    for (usize i = 0;i < indirection;i++) write_code(dest, "*");
}

void write_func(Module* module, FILE* dest, FunctionDef* func, GenericsMapping* mapping) {
    write_code(dest, "%s", func->name);
    if (mapping != NULL && func->generics.length > 0) {
        write_code(dest, "__", func->name);
        for (usize i = 0;i < mapping->length;i++) {
            TypeValue* tv = degenerify_mapping(mapping->elements[i]->type, mapping, mapping->elements[i]->type->src_line);
            write_type_inner(module, dest, find_type_def(module, tv), tv->indirection, &tv->generics, 0);
        }
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
    tdef->generics = (StrList)list_new();
    tdef->mappings = (GenericsMappingList)list_new();
    list_append(types, tdef);
}

void register_builtin_types(TypeDefList* types) {
    Struct* s = malloc(sizeof(Struct));
    s->name = copy_str("unit");
    s->fields = (StrList)list_new();
    s->fields_t = (TypeValueList)list_new();
    Type* type = malloc(sizeof(Type));
    type->type = TYPE_STRUCT;
    type->ty = s;
    TypeDef* tdef = malloc(sizeof(TypeDef));
    tdef->name = copy_str("unit");
    tdef->type = type;
    tdef->generics = (StrList)list_new();
    list_append(types, tdef);

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