#include "ast.h"
#include "lib.h"
#include "lib/defines.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
#include "token.h"
#include <stdio.h>

Path* path_new(bool absolute, IdentList elements) {
    Path* path = gc_malloc(sizeof(Path));
    path->absolute = absolute;
    path->elements = elements;
    return path;
} 

void path_append(Path* parent, Identifier* child) {
    list_append(&parent->elements, child);
}

Identifier* path_pop(Path* path) {
    return list_pop(&path->elements);
}

Path* path_join(Path* parent, Path* child) {
    if (child->absolute) panic("Cannot join an absolute path onto another!");
    Path* joined = path_new(parent->absolute, list_new(IdentList));
    list_extend(&joined->elements, &parent->elements);
    list_extend(&joined->elements, &child->elements);
    return joined;
}

void fprint_path(FILE* file, Path* path) {
    if (path->absolute) fprintf(file, "::");
    list_foreach_i(&path->elements, lambda(void, (usize i, Identifier* element) {
        if (i > 0) fprintf(file, "::");
        fprintf(file, "%s", element->name);
    }));
}

void fprint_expression(FILE* file, Expression* expression) {
    switch (expression->type) {
        default:
            unreachable("fprintf_expression %s", ExprType__NAMES[expression->type]);
    }
}

void fprint_typevalue(FILE* file, TypeValue* tval) {
    fprint_path(file, tval->name);
    if (tval->generics != NULL && tval->generics->generics.length > 0) {
        fputc('<', file);
        list_foreach_i(&tval->generics->generics, lambda(void, (int i, TypeValue* generic) {
            fprint_typevalue(file, generic);
        }));
        fputc('>', file);   
    }
}
