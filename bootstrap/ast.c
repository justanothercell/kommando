#include "ast.h"
#include "lib.h"
#include "lib/list.h"
#include "token.h"
#include "module.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

Path* path_new(bool absolute, IdentList elements) {
    Path* path = malloc(sizeof(Path));
    path->absolute = absolute;
    path->elements = elements;
    return path;
}

Path* path_simple(Identifier* name) {
    IdentList elements = list_new(IdentList);
    list_append(&elements, name);
    return path_new(false, elements);
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
    if (path->absolute && path->elements.length > 0) fprintf(file, "::");
    list_foreach_i(&path->elements, lambda(void, usize i, Identifier* element, {
        if (i > 0) fprintf(file, "::");
        fprintf(file, "%s", element->name);
    }));
}

void fprint_td_path(FILE* file, TypeDef* td) {
    if (td->module != NULL) {
        fprint_path(file, td->module->path);
        fprintf(file, "::");
    }
    fprintf(file, "%s", td->name->name);
}

void fprint_expression(FILE* file, Expression* expression) {
    switch (expression->type) {
        default:
            unreachable("fprintf_expression %s", ExprType__NAMES[expression->type]);
    }
}

void fprint_typevalue(FILE* file, TypeValue* tval) {
    if (tval == NULL) {
        fprintf(file, "(null)");
        return;
    }
    if (tval->def != NULL && tval->def->module != NULL) {
        fprint_path(file, tval->def->module->path);
        fprintf(file, "::%s", tval->def->name->name);
    } else{
        fprint_path(file, tval->name);        
    }
    if (tval->generics != NULL && tval->generics->generics.length > 0) {
        fputc('<', file);
        list_foreach_i(&tval->generics->generics, lambda(void, int i, TypeValue* generic, {
            if (i > 0) fprintf(file, ", ");
            fprint_typevalue(file, generic);
        }));
        fputc('>', file);   
    }
}
