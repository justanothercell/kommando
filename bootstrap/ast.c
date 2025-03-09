#include "ast.h"
#include "lib.h"
#include "lib/list.h"
#include "lib/map.h"
#include "resolver.h"
LIB;
#include "module.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

Path* path_new(bool absolute, IdentList elements) {
    Path* path = malloc(sizeof(Path));
    path->absolute = absolute;
    path->elements = list_new(IdentList);
    list_foreach(&elements, i, Identifier* ident, {
        list_append(&path->elements, ident);
    });
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
    list_foreach(&path->elements, i, Identifier* element, {
        if (i > 0) fprintf(file, "::");
        fprintf(file, "%s", element->name);
    });
}

void fprint_td_path(FILE* file, TypeDef* td) {
    if (td->module != NULL) {
        fprint_path(file, td->module->path);
        fprintf(file, "::");
    }
    fprintf(file, "%s", td->name->name);
}

void fprint_generic_keys(FILE* file, GenericKeys* keys) {
    if (keys == NULL || keys->generics.length == 0) return;
    fprintf(file, "<");
    list_foreach(&keys->generics, i, GKey* t, {
        if (i > 0) fprintf(file, ", ");
        fprintf(file, "%s", t->name->name);
        if (t->bounds.length > 0) {
            fprintf(file, ": ");
            list_foreach(&t->bounds, j, TraitBound* tb, {
                if (j > 0) fprintf(file, " + ");
                fprint_typevalue(file, tb->bound);
            });
        }
    });
    fprintf(file, ">");
}

void fprint_generic_values(FILE* file, GenericValues* values) {
    if (values == NULL || values->generics.length == 0) return;
    fprintf(file, "<");
    list_foreach(&values->generics, i, TypeValue* tv, {
        if (i > 0) fprintf(file, ", ");
        fprint_typevalue(file, tv);
    });
    fprintf(file, ">");
}

void fprint_type(FILE* file, TypeDef* def) {
    fprint_td_path(file, def);
    if (def->traits.length > 0) {
        fprintf(file, ": ");
        list_foreach(&def->traits, i, TraitBound* trait, {
            if (i > 0) fprintf(file, " + ");
            fprint_typevalue(file, trait->bound);
        });
    }
    fprint_generic_keys(file, def->generics);
}

void fprint_expression(FILE* file, Expression* expression) {
    UNUSED(file);
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
    /*if (map_size(tval->trait_impls) > 0) {
        fprintf(file, ": ");
        bool first = true;
        map_foreach(tval->trait_impls, str key, ImplBlock* impl, {
            UNUSED(key);
            if (!first) fprintf(file, " + ");
            fprint_path(file, impl->trait->module->path);
            fprintf(file, "::%s", impl->trait->name->name);
            first = false;
        });
    }*/
    if (tval->generics != NULL && tval->generics->generics.length > 0) {
        fputc('<', file);
        list_foreach(&tval->generics->generics, i, TypeValue* generic, {
            if (i > 0) fprintf(file, ", ");
            fprint_typevalue(file, generic);
        });
        fputc('>', file);   
    }
}

void fprint_full_typevalue(FILE* file, TypeValue* tval) {
    if (tval == NULL) {
        fprintf(file, "(null)");
        return;
    }
    bool has_impls = map_size(tval->trait_impls) > 0 || (tval->def != NULL && tval->def->traits.length > 0);
    if (has_impls) fprintf(file, "{");
    if (tval->def != NULL && tval->def->module != NULL) {
        fprint_path(file, tval->def->module->path);
        fprintf(file, "::%s", tval->def->name->name);
    } else{
        fprint_path(file, tval->name);        
    }
    if (has_impls) {
        fprintf(file, ": ");
        bool first = true;
        map_foreach(tval->trait_impls, str key, ImplBlock* impl, {
            UNUSED(key);
            if (!first) fprintf(file, " + ");
            fprint_full_typevalue(file, impl->trait_ref);
            first = false;
        });
        if (tval->def != NULL) {
            list_foreach(&tval->def->traits, i, TraitBound* impl, {
                if (i > 0) fprintf(file, " + ");
                fprint_full_typevalue(file, impl->bound);
                first = false;
            });
        }
        if (has_impls) fprintf(file, "}");
    }
    if (tval->generics != NULL && tval->generics->generics.length > 0) {
        fputc('<', file);
        list_foreach(&tval->generics->generics, i, TypeValue* generic, {
            if (i > 0) fprintf(file, ", ");
            fprint_full_typevalue(file, generic);
        });
        fputc('>', file);   
    }
}
