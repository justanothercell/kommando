#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <threads.h>
#include <time.h>

#include "compiler.h"
#include "lib.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
LIB
#include "transpiler_c.h"
#include "ast.h"
#include "module.h"
#include "parser.h"
#include "resolver.h"
#include "token.h"

void queue_type(Program* program, CompilerOptions* options, TypeValue* tv);
void queue_func(Program* program, CompilerOptions* options, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics);

void fprint_resolved_typevalue(FILE* stream, TypeValue* tv, GenericValues* type_generics, GenericValues* func_generics, bool long_names) {
    TypeDef* ty = tv->def;
    if (ty == NULL) panic("Unresolved type");
    if (ty->module == NULL) { // is generic
        TypeValue* type_candidate = NULL;
        if (type_generics != NULL) type_candidate = map_get(type_generics->resolved, ty->name->name);
        TypeValue* func_candidate = NULL;
        if (func_generics != NULL) func_candidate = map_get(func_generics->resolved, ty->name->name);
        if (type_candidate != NULL && func_candidate != NULL) spanned_error("Multiple generic candidates", ty->name->span, "%s @ %s from %s or %s @ %s from %s", to_str_writer(s, fprint_typevalue(s, type_candidate)), to_str_writer(s, fprint_span(s, &type_candidate->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &type_candidate->def->name->span)), 
            to_str_writer(s, fprint_typevalue(s, func_candidate)), to_str_writer(s, fprint_span(s, &func_candidate->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &func_candidate->def->name->span)));
        TypeValue* concrete = NULL;
        if (type_candidate != NULL) concrete = type_candidate;
        if (func_candidate != NULL) concrete = func_candidate;
        if (concrete != NULL) {
            return fprint_resolved_typevalue(stream, concrete, NULL, NULL, long_names);
        }
        spanned_error("Unsubsituted generic", ty->name->span, "Genric %s. This is probably a compiler error. %s with %s and %s", ty->name->name, gvals_to_key(tv->generics), gvals_to_key(type_generics), gvals_to_key(func_generics));
    }
    if (long_names) {
        fprint_path(stream, ty->module->path);
        fprintf(stream, "::%s", ty->name->name);
    } else {
        fprintf(stream, "%s", ty->name->name);
    }
    if (tv->generics != NULL && tv->generics->generics.length > 0) {
        fprintf(stream, "<");
        list_foreach(&tv->generics->generics, i, TypeValue* v, {
            if (i > 0) fprintf(stream, ", ");
            fprint_resolved_typevalue(stream, v, type_generics, func_generics, long_names);
        });
        fprintf(stream, ">");
    }
}

static void fprint_str_lit(FILE* stream, str lit) {
    fputc('"', stream);
    char c = *lit;
    while(c) {
        if (c == '\\') fputs("\\\\", stream);
        else if (c == '\n') fputs("\\n", stream);
        else if (c == '\r') fputs("\\r", stream);
        else if (c == '\t') fputs("\\t", stream);
        else if (c == '"') fputs("\\\"", stream);
        else if (c <= 31 || c == 127) fprintf(stream, "\\x%02x", c);
        else fputc(c, stream);
        c = *++lit;
    }
    fputc('"', stream);
}
TypeDef* PTR_DEF = NULL;
str gen_c_type_name(Program* program, CompilerOptions* options, TypeValue* tv, GenericValues* type_ctx, GenericValues* func_ctx);
static str gen_c_type_name_inner(Program* program, CompilerOptions* options, TypeValue* tv, GenericValues* type_generics, GenericValues* func_generics, bool toplevel) {
    TypeDef* ty = tv->def;
    if (ty == PTR_DEF) {
        return to_str_writer(s, {
            fprintf(s, "%s*", gen_c_type_name(program, options, tv->generics->generics.elements[0], type_generics, func_generics));
        });
    }
    if (ty == NULL) spanned_error("Unresolved type", tv->name->elements.elements[0]->span,  "Couldn't resolve %s", to_str_writer(s, fprint_typevalue(s, tv)));
    if (ty->extern_ref != NULL && toplevel) {
        return ty->extern_ref;
    }
    if (ty->module == NULL) {
        TypeValue* type_candidate = NULL;
        if (type_generics != NULL) type_candidate = map_get(type_generics->resolved, ty->name->name);
        TypeValue* func_candidate = NULL;
        if (func_generics != NULL) func_candidate = map_get(func_generics->resolved, ty->name->name);
        if (type_candidate != NULL && func_candidate != NULL) spanned_error("Multiple generic candidates", ty->name->span, "%s @ %s from %s or %s @ %s from %s", to_str_writer(s, fprint_typevalue(s, type_candidate)), to_str_writer(s, fprint_span(s, &type_candidate->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &type_candidate->def->name->span)), 
            to_str_writer(s, fprint_typevalue(s, func_candidate)), to_str_writer(s, fprint_span(s, &func_candidate->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &func_candidate->def->name->span)));
        TypeValue* concrete = NULL;
        if (type_candidate != NULL) concrete = type_candidate;
        if (func_candidate != NULL) concrete = func_candidate;
        if (concrete == NULL) {
            if (func_generics != NULL && type_generics != NULL) spanned_error("Unsubsituted generic", ty->name->span, "Cannot substitute `%s` with a concrete type from %s @ %s or %s @ %s", ty->name->name, gvals_to_key(func_generics), to_str_writer(s, fprint_span(s, &func_generics->span)), gvals_to_key(type_generics), to_str_writer(s, fprint_span(s, &type_generics->span)));
            if (func_generics != NULL) spanned_error("Unsubsituted generic", ty->name->span, "Cannot substitute `%s` with a concrete type from %s @ %s or %s @ %s", ty->name->name, gvals_to_key(func_generics), to_str_writer(s, fprint_span(s, &func_generics->span)));
            if (type_generics != NULL) spanned_error("Unsubsituted generic", ty->name->span, "Cannot substitute `%s` with a concrete type from %s @ %s or %s @ %s", ty->name->name, gvals_to_key(type_generics), to_str_writer(s, fprint_span(s, &type_generics->span)));
            spanned_error("Unsubsituted generic", ty->name->span, "Cannot substitute `%s` with a concrete as there is nothing to subsitute from", ty->name->name);
        }
        if (concrete->def != tv->def) {
            return gen_c_type_name_inner(program, options, concrete, func_generics, type_generics, toplevel);
        }
    }
    if (tv->ctx != NULL) spanned_error("Indirect generic type", ty->name->span, "Type %s is not concrete", ty->name->name);
    if (ty->module == NULL) {
        if (func_generics != NULL) spanned_error("Unsubsituted generic", ty->name->span, "Genric %s. This is probably a compiler error. %s with %s @ %s", ty->name->name, gvals_to_key(tv->generics), gvals_to_key(func_generics), to_str_writer(s, fprint_span(s, &func_generics->span)));
        else spanned_error("Unsubsituted NULL", ty->name->span, "Genric %s. This is probably a compiler error. %s with %s", ty->name->name, gvals_to_key(tv->generics), gvals_to_key(func_generics));
    }
    str name = NULL;
    int state = 0;
    list_foreach(&tv->def->annotations, i, Annotation a, {
        str p = to_str_writer(s, fprint_path(s, a.path));
        if (str_eq(p, "symbol")) {
            if (state == 1) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "Symbol name already defined");
            if (state == 2) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "Cannot both define symbol name and no_mangle");
            if (a.type != AT_DEFINE) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected define `#[symbol=\"foo\"]`, found %s", AnnotationType__NAMES[a.type]);
            if (tv->def->generics != NULL && tv->def->generics->generics.length > 0) spanned_error("Unmangled generic", a.path->elements.elements[0]->span, "a custom symbol cannot be set for a generic type");
            Token* t = a.data;
            if (t->type != STRING) unexpected_token(t, "Annotation expects string value");
            name = t->string;
            state = 1;
        }
        if (str_eq(p, "no_mangle")) {
            if (state == 1) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "Cannot both define symbol name and no_mangle");
            if (state == 2) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "no_mangle already set");
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[no_mangle]`, found %s", AnnotationType__NAMES[a.type]);
            if (tv->def->generics != NULL && tv->def->generics->generics.length > 0) spanned_error("Unmangled generic", a.path->elements.elements[0]->span, "no_mangle cannot be set for a generic type");
            name = tv->def->name->name;
            state = 2;
        }
    });
    if (name != NULL) return to_str_writer(s, fprintf(s, "struct %s", name));
    
    str key = to_str_writer(stream, {
        fprintf(stream, "%p", ty);
        if (tv->generics != NULL) {
            fprintf(stream, "<");
            list_foreach(&tv->generics->generics, i, TypeValue* generic_tv, {
                if (i > 0) fprintf(stream, ",");
                fprintf(stream, "%s", gen_c_type_name_inner(program, options,generic_tv, func_generics, type_generics, false));
            });
            fprintf(stream, ">");
        }
    });
    u32 hash = str_hash(key);
    return to_str_writer(stream, fprintf(stream,"struct %s%lx", ty->name->name, hash));
}
str gen_c_type_name(Program* program, CompilerOptions* options, TypeValue* tv, GenericValues* type_ctx, GenericValues* func_ctx) {
    queue_type(program, options, replace_generic(program, options, tv, type_ctx, func_ctx, NULL, NULL));
    return gen_c_type_name_inner(program, options, tv, type_ctx, func_ctx, true);
}

str gen_default_c_fn_name(Program* program, CompilerOptions* options, FuncDef* def, GenericValues* type_ctx, GenericValues* func_ctx) {
    queue_func(program, options, def, type_ctx, func_ctx);
    str data = to_str_writer(s, {
        fprintf(s, "%p%s", def, tfvals_to_key(type_ctx, func_ctx));
    });
    u32 hash = str_hash(data);
    if (def->impl_type == NULL) {
        return to_str_writer(s, fprintf(s,"%s%lx", def->name->name, hash));
    } else {
        return to_str_writer(s, fprintf(s,"%s_%s%lx", def->impl_type->def->name->name, def->name->name, hash));
    }
}

str gen_c_fn_name(Program* program, CompilerOptions* options, FuncDef* def, GenericValues* type_generics, GenericValues* func_generics) {
    queue_func(program, options, def, type_generics, func_generics);
    if (def->trait_def) spanned_error("Trait definition transpilation", def->name->span, "Functions in trait definitions do not get transpiled. This is a compiler error.");
    if (def->body == NULL) {
        if (def->generics != NULL) spanned_error("Generic extern function", def->name->span, "Extern functions may not be generic");
        return def->name->name;
    }
    str name = NULL;
    int state = 0;
    list_foreach(&def->annotations, i, Annotation a, {
        str p = to_str_writer(s, fprint_path(s, a.path));
        if (str_eq(p, "symbol")) {
            if (state == 1) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "Symbol name already defined");
            if (state == 2) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "Cannot both define symbol name and no_mangle");
            if (a.type != AT_DEFINE) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected define `#[symbol=\"foo\"]`, found %s", AnnotationType__NAMES[a.type]);
            if (def->generics != NULL && def->generics->generics.length > 0) spanned_error("Unmangled generic", a.path->elements.elements[0]->span, "a custom symbol cannot be set for a generic function");
            Token* t = a.data;
            if (t->type != STRING) unexpected_token(t, "Annotation expects string value");
            name = t->string;
            state = 1;
        }
        if (str_eq(p, "no_mangle")) {
            if (state == 1) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "Cannot both define symbol name and no_mangle");
            if (state == 2) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "no_mangle already set");
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[no_mangle]`, found %s", AnnotationType__NAMES[a.type]);
            if (def->generics != NULL && def->generics->generics.length > 0) spanned_error("Unmangled generic", a.path->elements.elements[0]->span, "no_mangle cannot be set for a generic function");
            if (def->impl_type == NULL) {
                name = def->name->name;
            } else {
                name = to_str_writer(s, fprintf(s,"%s_%s", def->impl_type->def->name->name, def->name->name));
            }
            state = 2;
        }
    });
    if (name != NULL) return name;
    return gen_default_c_fn_name(program, options, def, type_generics, func_generics);
}

static bool contains_self(TypeValue* value, TypeValue* self) {
    if (value->def == self->def) return true;
    if (value->generics != NULL) {
        list_foreach(&value->generics->generics, i, TypeValue* g, {
           if (contains_self(g, self)) return true; 
        });
    }
    return false;
}
GenericValues* expand_generics(GenericValues* generics, GenericValues* type_context, GenericValues* func_context) {
    if (generics == NULL) panic("cannot expand null generics");
    if (type_context == NULL && func_context == NULL) return generics;
    GenericValues* expanded = malloc(sizeof(GenericValues));
    expanded->resolved = map_new();
    expanded->span = generics->span;
    expanded->generics = list_new(TypeValueList);
    Map* rev = map_new();
    map_foreach(generics->resolved, str key, TypeValue* val, {
        if (str_eq(to_str_writer(s, fprint_typevalue(s, val)), "_")) spanned_error("Unresolved generic", val->name->elements.elements[0]->span, "`_`");
        map_put(rev, to_str_writer(s, fprintf(s, "%p", val)), key);
    });
    list_foreach(&generics->generics, i, TypeValue* generic, {
        if (generic->def->traits.length > 0) {
//log("%s %s - %s %s", to_str_writer(s, fprint_typevalue(s, generic)), to_str_writer(s, fprint_span(s, &generic->name->elements.elements[0]->span)), to_str_writer(s, fprint_type(s, generic->def)), to_str_writer(s, fprint_span(s, &generic->def->name->span)));
        }
        str key = to_str_writer(stream, fprint_typevalue(stream, generic));
        str real_key = map_get(rev, to_str_writer(s, fprintf(s, "%p", generic)));
        TypeValue* concrete1 = NULL;
        if (type_context != NULL) concrete1 = map_get(type_context->resolved, key);
        TypeValue* concrete2 = NULL;
        if (func_context != NULL)  concrete2 = map_get(func_context->resolved, key);
        if (concrete1 != NULL && concrete2 != NULL) spanned_error("Multiple generic candidates", generic->name->elements.elements[0]->span, "%s @ %s from %s or %s @ %s from %s", to_str_writer(s, fprint_typevalue(s, concrete1)), to_str_writer(s, fprint_span(s, &concrete1->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &concrete1->def->name->span)), 
            to_str_writer(s, fprint_typevalue(s, concrete2)), to_str_writer(s, fprint_span(s, &concrete2->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &concrete2->def->name->span)));
        TypeValue* concrete = NULL;
        if (concrete1 != NULL) concrete = concrete1;
        if (concrete2 != NULL) concrete = concrete2;
        if (concrete != NULL) {
            TypeValue* concrete_copy = malloc(sizeof(TypeValue));
            concrete_copy->ctx = concrete->ctx;
            concrete_copy->def = concrete->def;
            concrete_copy->generics = concrete->generics;
            concrete_copy->name = concrete->name;
            concrete_copy->trait_impls = concrete->trait_impls;
            
            if (contains_self(concrete_copy, generic)) return NULL;

            list_append(&expanded->generics, concrete_copy);
            map_put(expanded->resolved, real_key, concrete_copy);
        } else {
            TypeValue* concrete = generic;
            GenericValues* c_e = concrete->generics;
            if (c_e != NULL) {
                c_e = expand_generics(concrete->generics, type_context, func_context);
                if (c_e == NULL) return NULL;
            }
            TypeValue* concrete_expanded = malloc(sizeof(TypeValue));
            concrete_expanded->ctx = NULL;
            concrete_expanded->generics = c_e;
            concrete_expanded->def = concrete->def;
            concrete_expanded->name = concrete->name;
            concrete_expanded->trait_impls = concrete->trait_impls;

            list_append(&expanded->generics, concrete_expanded);
            map_put(expanded->resolved, real_key, concrete_expanded);
        }
    });
    return expanded;
}

str gen_c_static_name(Global* s) {
    if (s->constant) panic("Expected static, got constant. This is a compiler error");
    str name = NULL;
    int state = 0;
    list_foreach(&s->annotations, i, Annotation a, {
        str p = to_str_writer(s, fprint_path(s, a.path));
        if (str_eq(p, "symbol")) {
            if (state == 1) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "Symbol name already defined");
            if (state == 2) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "Cannot both define symbol name and no_mangle");
            if (a.type != AT_DEFINE) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected define `#[symbol=\"foo\"]`, found %s", AnnotationType__NAMES[a.type]);
            Token* t = a.data;
            if (t->type != STRING) unexpected_token(t, "Annotation expects string value");
            name = t->string;
            state = 1;
        } if (str_eq(p, "no_mangle")) {
            if (state == 1) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "Cannot both define symbol name and no_mangle");
            if (state == 2) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "`#[no_mangle]` already set");
            if (state == 3) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "flag extern implies flag no_mangle");
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[no_mangle]`, found %s", AnnotationType__NAMES[a.type]);
            name = s->name->name;
            state = 2;
        } if (str_eq(p, "extern")) {
            if (state == 2) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "flag extern implies flag no_mangle");
            if (state == 3) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "`#[extern]` already set");
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[extern]`, found %s", AnnotationType__NAMES[a.type]);
            if (name == NULL) name = s->name->name;
            state = 3;
        }
    });
    if (name == NULL) {
        name = to_str_writer(stream, { 
            fprintf(stream, "%s%llx", s->name->name, (usize)s);
        });
    }
    return name;
}

str gen_c_varbox_name(VarBox* box) {
    return to_str_writer(stream, fprintf(stream,"%s%llx", box->name, box->id));
}

str gen_c_var_name(Program* program, CompilerOptions* options, Variable* v, GenericValues* type_generics, GenericValues* func_generics) {
    if (v->global_ref != NULL) {
        if (v->global_ref->constant) {
            if (v->global_ref->computed_value->type == STRING) {
                return to_str_writer(s, fprint_str_lit(s, v->global_ref->computed_value->string));
            } else {
                return v->global_ref->computed_value->string;
            }
        } 
        return gen_c_static_name(v->global_ref);
    }
    if (v->box->name != NULL) {
        return gen_c_varbox_name(v->box);
    } else {
        if (v->method_name != NULL) {
            FuncDef* func = v->box->mi->item;
            if (func->trait != NULL) {
                TypeValue* actual = replace_generic(program, options, v->box->ty, func_generics, type_generics, NULL, NULL);
                ImplBlock* trait_impl = map_get(actual->trait_impls, to_str_writer(s, fprintf(s, "%p", func->trait)));
                if (trait_impl == NULL) panic("No trait impl for %s on %s found", func->trait->name->name, to_str_writer(s, fprint_typevalue(s, actual)));
                ModuleItem* method = map_get(trait_impl->methods, func->name->name);
                if (method == NULL) panic("No method in trait impl found");
                if (method->type != MIT_FUNCTION) panic("is not a method");
                func = method->item;
            }
            GenericValues* func_gens = v->method_values;
            if (func_gens != NULL) func_gens = expand_generics(func_gens, type_generics, func_generics);
            GenericValues* ty_gens = expand_generics(v->values, type_generics, func_generics);
            return gen_c_fn_name(program, options, func, ty_gens, func_gens);
        } else {
            switch (v->box->mi->type) {
                case MIT_FUNCTION: {
                    FuncDef* func = v->box->mi->item;
                    GenericValues* func_gens = v->values;
                    if (func_gens != NULL) func_gens = expand_generics(func_gens, type_generics, func_generics);
                    return gen_c_fn_name(program, options, func, NULL, func_gens);
                } break;
                default:
                    unreachable();
            }
        }
    }
}

u32 TEMP_COUNTER = 0;
void reset_temp_context() {
    TEMP_COUNTER = 0;
}
str gen_temp_c_name(str name) {
    return to_str_writer(stream, fprintf(stream, "%s%lxt", name, TEMP_COUNTER++));
}

static void transpile_drop_rec(Program* program, CompilerOptions* options, FILE* code_stream, TypeValue* tv, str c_object) {
    ImplBlock* drop = map_get(tv->trait_impls, program->raii.drop_key);
    if (drop != NULL) {
        ModuleItem* drop_it = map_get(drop->methods, "drop");
        FuncDef* drop_fn = drop_it->item;
        str drop_name = gen_c_fn_name(program, options, drop_fn, tv->generics, NULL);
        fprintf(code_stream, "    %s(&%s);\n", drop_name, c_object);
    }
    if (tv->def->fields != NULL) {
        list_foreach(&tv->def->flist, i, Identifier* fname, {
            Field* field = map_get(tv->def->fields, fname->name);
            TypeValue* field_ty = replace_generic(program, options, field->type, tv->generics, NULL, NULL, NULL);
            if (map_get(field_ty->trait_impls, program->raii.copy_key)) continue;
            if (!map_get(field_ty->trait_impls, program->raii.drop_key) && field_ty->def->flist.length == 0) continue;
            str c_field_var = gen_temp_c_name(field->name->name);
            str c_field_ty = gen_c_type_name(program, options, field->type, tv->generics, NULL);
            fprintf(code_stream, "    %s %s = %s.%s;\n", c_field_ty, c_field_var, c_object, field->name->name);
            transpile_drop_rec(program, options, code_stream, field_ty, c_field_var);
        });
    }
}

void transpile_drop(Program* program, CompilerOptions* options, FILE* code_stream, GenericValues* type_generics, GenericValues* func_generics, TypeValue* generic_type, str c_object) {
    TypeValue* concrete = replace_generic(program, options, generic_type, type_generics, func_generics, NULL, NULL);
    if (map_get(concrete->trait_impls, program->raii.copy_key)) return;
    if (options->emit_info) fprintf(code_stream, "    // drop %s: %s\n", c_object, to_str_writer(s, fprint_typevalue(s, generic_type)));
    transpile_drop_rec(program, options, code_stream, concrete, c_object);
}

typedef struct DropItem {
    TypeValue* tv;
    str c_var;
} DropItem;
LIST(DropItemList, DropItem);

void transpile_block(Program* program, CompilerOptions* options, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Block* block, str return_var, bool pass_ref, str continue_label, str break_label);
void transpile_expression(Program* program, CompilerOptions* options, DropItemList* dil, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Expression* expr, str return_var, bool take_ref, str continue_label, str break_label) {
    //log("'%s' %s", ExprType__NAMES[expr->type], to_str_writer(s, fprint_span(s, &expr->span)));
    if (options->emit_info) {
        fprintf(code_stream, "    // ");
        fprint_span(code_stream, &expr->span);
        fprintf(code_stream, "\n");
    }
    bool is_dropped = false;
    if (return_var == NULL) {
        TypeValue* yield_ty = replace_generic(program, options, expr->resolved->type, type_generics, func_generics, NULL, NULL);
        if (!map_contains(yield_ty->trait_impls, program->raii.copy_key)) {
            is_dropped = true;
            return_var = gen_temp_c_name("dropped");
            fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
        }
    }
    str original_return_var = return_var;
    switch (expr->type) {
        case EXPR_BIN_OP: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            BinOp* op = expr->expr;
            str lhs = NULL;
            if (op->lhs->type == EXPR_VARIABLE) {
                lhs = gen_c_var_name(program, options, (Variable*)op->lhs->expr, type_generics, func_generics);
            } else {
                lhs = gen_temp_c_name("lhs");
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, op->lhs->resolved->type, type_generics, func_generics), lhs);
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, op->lhs, lhs, false, continue_label, break_label);
            }
            str rhs = NULL;
            if (op->rhs->type == EXPR_VARIABLE) {
                rhs = gen_c_var_name(program, options, (Variable*)op->rhs->expr, type_generics, func_generics);
            } else {
                rhs = gen_temp_c_name("rhs");
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, op->rhs->resolved->type, type_generics, func_generics), rhs);
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, op->rhs, rhs, false, continue_label, break_label);
            }
            fprintf(code_stream, "    ");
            fprintf(code_stream, "%s = ", return_var);
            fprintf(code_stream, "%s %s %s;\n", lhs, op->op, rhs);
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_BIN_OP_ASSIGN: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            BinOp* op = expr->expr;
            str ref = gen_temp_c_name("ref");
            str ty = gen_c_type_name(program, options, op->lhs->resolved->type, type_generics, func_generics);
            str rhs = gen_temp_c_name("rhs");
            fprintf(code_stream, "    %s* %s;\n", ty, ref);
            fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, op->rhs->resolved->type, type_generics, func_generics), rhs);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, op->lhs, ref, true, continue_label, break_label);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, op->rhs, rhs, false, continue_label, break_label);
            fprintf(code_stream, "    *%s = *%s %s %s;\n", ref, ref, op->op, rhs);

            if (return_var != NULL) fprintf(code_stream, "    %s = (%s) {};\n", return_var, gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics));
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_FUNC_CALL: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            FuncCall* fc = expr->expr;
            FuncDef* fd = fc->def;
            GenericValues* fc_generics = fc->generics;
            if (fc->generics != NULL) {
                fc_generics = expand_generics(fc_generics, type_generics, func_generics);
            }
            str c_fn_name = gen_c_fn_name(program, options, fd, NULL, fc_generics);
            Module* root = fd->module;
            while (root->parent != NULL) { root = root->parent; }

            StrList argnames = list_new(StrList);
            list_foreach(&fc->arguments, i, Expression* arg, {
                if (arg->type != EXPR_VARIABLE) {
                    str argname = gen_temp_c_name("arg");
                    fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, arg->resolved->type, type_generics, func_generics), argname);
                    transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, arg, argname, false, continue_label, break_label);
                    list_append(&argnames, argname);
                } else {
                    list_append(&argnames, gen_c_var_name(program, options, (Variable*)arg->expr, type_generics, func_generics));
                }
            });
            str local_frame_name = NULL;
            if (program->tracegen.trace_this && !fd->untraced) {
                local_frame_name = gen_temp_c_name("local_frame");
                fprintf(code_stream, "    %s %s = { .parent=%s, .call=&TRACE_%s, .loc={ .file=\"%s\", .line=%lld }, .callflags=", program->tracegen.frame_type_c_name, local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(program, options, fd, NULL, fc_generics), fc->name->elements.elements[0]->span.left.file, fc->name->elements.elements[0]->span.left.line);
                if (fd->body == NULL) fprintf(code_stream, "0b110");
                else if (func->module == program->main_module && root != program->main_module && options->tracelevel < 2) fprintf(code_stream, "0b100");
                else fprintf(code_stream, "0b0");
                fprintf(code_stream, " };\n");
                fprintf(code_stream, "    %s = &%s;\n", program->tracegen.top_frame_c_name, local_frame_name);
            }
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach(&argnames, i, str arg, {
                if (i > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, "%s", arg);
            });
            fprintf(code_stream, ");\n");
            if (program->tracegen.trace_this && !fd->untraced) fprintf(code_stream, "    %s = %s.parent;\n", program->tracegen.top_frame_c_name, local_frame_name);
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_METHOD_CALL: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            MethodCall* call = expr->expr;
            FuncDef* fd = call->def;
            if (fd->trait != NULL) {
                TypeValue* actual = replace_generic(program, options, call->tv, func_generics, type_generics, NULL, NULL);
                ImplBlock* trait_impl = map_get(actual->trait_impls, to_str_writer(s, fprintf(s, "%p", call->def->trait)));
                if (trait_impl == NULL) spanned_error("No trait impl found", expr->span, "This is probably a compiler error");
                ModuleItem* method = map_get(trait_impl->methods, fd->name->name);
                if (method == NULL) spanned_error("No method in trait impl found", expr->span, "This is probably a compiler error");
                if (method->type != MIT_FUNCTION) spanned_error("is not a method", expr->span, "This is probably a compiler error");
                fd = method->item;
            }
            GenericValues* call_generics = expand_generics(call->generics, type_generics, func_generics);
            GenericValues* type_call_generics = expand_generics(call->impl_vals, type_generics, func_generics);
            str c_fn_name = gen_c_fn_name(program, options, fd, type_call_generics, call_generics);
            Module* root = fd->module;
            while (root->parent != NULL) { root = root->parent; }
            
            StrList argnames = list_new(StrList);
            if (call->object->type != EXPR_VARIABLE) {
                str thisname = gen_temp_c_name("this");
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, call->object->resolved->type, type_generics, func_generics), thisname);
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, call->object, thisname, false, continue_label, break_label);
                list_append(&argnames, thisname);
            } else {
                list_append(&argnames, gen_c_var_name(program, options, (Variable*)call->object->expr, type_generics, func_generics));
            }
            list_foreach(&call->arguments, i, Expression* arg, {
                if (arg->type != EXPR_VARIABLE) {
                    str argname = gen_temp_c_name("arg");
                    fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, arg->resolved->type, type_generics, func_generics), argname);
                    transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, arg, argname, false, continue_label, break_label);
                    list_append(&argnames, argname);
                } else {
                    list_append(&argnames, gen_c_var_name(program, options, (Variable*)arg->expr, type_generics, func_generics));
                }
            });
            str local_frame_name = NULL;
            if (program->tracegen.trace_this && !fd->untraced) {
                local_frame_name = gen_temp_c_name("local_frame");
                fprintf(code_stream, "    %s %s = { .parent=%s, .call=&TRACE_%s, .loc={ .file=\"%s\", .line=%lld }, .callflags=", program->tracegen.frame_type_c_name, local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(program, options, fd, type_call_generics, call_generics), call->name->span.left.file, call->name->span.left.line);
                if (fd->body == NULL) fprintf(code_stream, "0b110");
                else if (func->module == program->main_module && root != program->main_module && options->tracelevel < 2) fprintf(code_stream, "0b100");
                else fprintf(code_stream, "0b0");
                fprintf(code_stream, " };\n");
                fprintf(code_stream, "    %s = &%s;\n", program->tracegen.top_frame_c_name, local_frame_name);
            }
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach(&argnames, i, str arg, {
                if (i > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, "%s", arg);
            });
            fprintf(code_stream, ");\n");
            if (program->tracegen.trace_this && !fd->untraced) fprintf(code_stream, "    %s = %s.parent;\n", program->tracegen.top_frame_c_name, local_frame_name);
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_STATIC_METHOD_CALL: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            StaticMethodCall* call = expr->expr;
            FuncDef* fd = call->def;
            if (fd->trait != NULL) {
                TypeValue* actual = replace_generic(program, options, call->tv, func_generics, type_generics, NULL, NULL);
                ImplBlock* trait_impl = map_get(actual->trait_impls, to_str_writer(s, fprintf(s, "%p", call->def->trait)));
                if (trait_impl == NULL) spanned_error("No trait impl found", expr->span, "This is probably a compiler error");
                ModuleItem* method = map_get(trait_impl->methods, fd->name->name);
                if (method == NULL) spanned_error("No method in trait impl found", expr->span, "This is probably a compiler error");
                if (method->type != MIT_FUNCTION) spanned_error("is not a method", expr->span, "This is probably a compiler error");
                fd = method->item;
            }
            GenericValues* call_generics = expand_generics(call->generics, type_generics, func_generics);
            GenericValues* type_call_generics = expand_generics(call->impl_vals, type_generics, func_generics);
            str c_fn_name = gen_c_fn_name(program, options, fd, type_call_generics, call_generics);
            Module* root = fd->module;
            while (root->parent != NULL) { root = root->parent; }
            
            StrList argnames = list_new(StrList);
            list_foreach(&call->arguments, i, Expression* arg, {
                if (arg->type != EXPR_VARIABLE) {
                    str argname = gen_temp_c_name("arg");
                    fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, arg->resolved->type, type_generics, func_generics), argname);
                    transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, arg, argname, false, continue_label, break_label);
                    list_append(&argnames, argname);
                } else {
                    list_append(&argnames, gen_c_var_name(program, options, (Variable*)arg->expr, type_generics, func_generics));
                }
            });
            str local_frame_name = NULL;
            if (program->tracegen.trace_this && !fd->untraced) {
                local_frame_name = gen_temp_c_name("local_frame");
                fprintf(code_stream, "    %s %s = { .parent=%s, .call=&TRACE_%s, .loc={ .file=\"%s\", .line=%lld }, .callflags=", program->tracegen.frame_type_c_name, local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(program, options, fd, type_call_generics, call_generics), call->name->span.left.file, call->name->span.left.line);
                if (fd->body == NULL) fprintf(code_stream, "0b110");
                else if (func->module == program->main_module && root != program->main_module && options->tracelevel < 2) fprintf(code_stream, "0b100");
                else fprintf(code_stream, "0b0");
                fprintf(code_stream, " };\n");
                fprintf(code_stream, "    %s = &%s;\n", program->tracegen.top_frame_c_name, local_frame_name);
            }
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach(&argnames, i, str arg, {
                if (i > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, "%s", arg);
            });
            fprintf(code_stream, ");\n");
            if (program->tracegen.trace_this && !fd->untraced) fprintf(code_stream, "    %s = %s.parent;\n", program->tracegen.top_frame_c_name, local_frame_name);
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_DYN_RAW_CALL: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            DynRawCall* fc = expr->expr;
            str c_ret = gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics);

            StrList argnames = list_new(StrList);
            list_foreach(&fc->args, i, Expression* arg, {
                if (arg->type != EXPR_VARIABLE) {
                    str argname = gen_temp_c_name("arg");
                    fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, arg->resolved->type, type_generics, func_generics), argname);
                    transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, arg, argname, false, continue_label, break_label);
            list_append(&argnames, argname);
                } else {
                    list_append(&argnames, gen_c_var_name(program, options, (Variable*)arg->expr, type_generics, func_generics));
                }
            });
            str local_frame_name = NULL;
            if (program->tracegen.trace_this) {
                local_frame_name = gen_temp_c_name("local_frame");
                fprintf(code_stream, "    %s %s = { .parent=%s, .call=0, .loc={ .file=\"%s\", .line=%lld }, .callflags=0b1000 };\n", program->tracegen.frame_type_c_name, local_frame_name, program->tracegen.top_frame_c_name, expr->span.left.file, expr->span.left.line);
                fprintf(code_stream, "    %s = &%s;\n", program->tracegen.top_frame_c_name, local_frame_name);
            }
            str funcptr = gen_temp_c_name("funcptr");
            fprintf(code_stream, "    void* %s;", funcptr);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, fc->callee, funcptr, false, continue_label, break_label);
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            fprintf(code_stream, "((%s(*)())%s)(", c_ret, funcptr);
            list_foreach(&argnames, i, str arg, {
                if (i > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, "%s", arg);
            });
            fprintf(code_stream, ");\n");
            if (program->tracegen.trace_this) fprintf(code_stream, "%s = %s.parent;\n", program->tracegen.top_frame_c_name, local_frame_name);
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_LITERAL: {
            // we assume that literals all have a trivial drop
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING:
                    fprint_str_lit(code_stream, lit->string);
                    break;
                case NUMERAL: {
                    int end = strlen(lit->string);
                    for (usize i = 0;i < strlen(lit->string);i++) {
                        if (lit->string[i] == 'u' || lit->string[i] == 'i') {
                            end = i;
                            break;
                        }
                    }
                    fprintf(code_stream, "%.*s", end, lit->string);
                } break;
                default:
                    unreachable("%s", TokenType__NAMES[lit->type]);
            }
            fprintf(code_stream, ";\n");
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_BLOCK: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            transpile_block(program, options, code_stream, func, type_generics, func_generics, expr->expr, return_var, take_ref, continue_label, break_label);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_VARIABLE: {
            Variable* v = expr->expr;
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            if (take_ref) fprintf(code_stream, "&");
            fprintf(code_stream, "%s;\n", gen_c_var_name(program, options, v, type_generics, func_generics));
        } break;
        case EXPR_LET: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            LetExpr* let = expr->expr;
            str c_ty = gen_c_type_name(program, options, let->type, type_generics, func_generics);
            str c_name = gen_c_var_name(program, options, let->var, type_generics, func_generics);
            fprintf(code_stream, "    %s %s;\n", c_ty, c_name);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, let->value, c_name, false, continue_label, break_label);
            if (return_var != NULL) fprintf(code_stream, "    %s = (%s) {};\n", return_var, gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics));
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_CONDITIONAL: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            Conditional* cond = expr->expr;
            str cond_ty = gen_c_type_name(program, options, cond->cond->resolved->type, type_generics, func_generics);
            str cond_var = gen_temp_c_name("cond");
            str else_label = gen_temp_c_name("else");
            str end_label = gen_temp_c_name("if_end");
            fprintf(code_stream, "    %s %s;\n", cond_ty, cond_var);
            transpile_expression(program, options, dil, code_stream, func,  type_generics, func_generics, cond->cond, cond_var, false, continue_label, break_label);
            fprintf(code_stream, "    if (!%s) goto %s;\n", cond_var, else_label);
            transpile_block(program, options, code_stream, func,  type_generics, func_generics, cond->then, return_var, take_ref, continue_label, break_label);
            fprintf(code_stream, "    goto %s;\n", end_label);
            fprintf(code_stream, "%s: {}\n", else_label);
            if (cond->otherwise != NULL) transpile_block(program, options, code_stream, func,  type_generics, func_generics, cond->otherwise, return_var, take_ref, continue_label, break_label);
            fprintf(code_stream, "    goto %s;\n", end_label);
            fprintf(code_stream, "%s: {}\n", end_label);
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_WHILE_LOOP: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            WhileLoop* wl = expr->expr;
            str cond_ty = gen_c_type_name(program, options, wl->cond->resolved->type, type_generics, func_generics);
            str cond_var = gen_temp_c_name("cond");
            str loop_label = gen_temp_c_name("while");
            str end_label = gen_temp_c_name("while_end");
            fprintf(code_stream, "    %s %s;\n", cond_ty, cond_var);
            fprintf(code_stream, "%s: {}\n", loop_label);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, wl->cond, cond_var, false, NULL, NULL);
            fprintf(code_stream, "    if (!%s) goto %s;\n", cond_var, end_label);
            transpile_block(program, options, code_stream, func, type_generics, func_generics, wl->body, return_var, false, loop_label, end_label);
            fprintf(code_stream, "    goto %s;\n", loop_label);
            fprintf(code_stream, "%s: {}\n", end_label);
            if (return_var != NULL) fprintf(code_stream, "    %s = (%s) {};\n", return_var, gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics));
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_RETURN: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            Expression* ret = expr->expr;
            str ret_ty = NULL;
            if (expr->expr == NULL) ret_ty = gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics);
            else ret_ty = gen_c_type_name(program, options, ret->resolved->type, type_generics, func_generics);
            str ret_var = gen_temp_c_name("return");
            fprintf(code_stream, "    %s %s;\n", ret_ty, ret_var);
            if (expr->expr == NULL) {
                fprintf(code_stream, "    %s = (%s) {};\n", ret_var, ret_ty);
            } else {
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, expr->expr, ret_var, false, continue_label, break_label);
            }
            fprintf(code_stream, "    return %s;\n", ret_var);
            if (return_var != NULL) fprintf(code_stream, "    %s = (%s) {};\n", return_var, gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics));
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_BREAK: {
            if (break_label == NULL) spanned_error("Break outside loop", expr->span, "Break needs to be inside loop");
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            if (expr->expr != NULL) todo("EXPR_BREAK with expr != NULL");
            fprintf(code_stream, "    goto %s;", break_label);
            if (return_var != NULL) fprintf(code_stream, "    %s = (%s) {};\n", return_var, gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics));
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_CONTINUE: {
            if (continue_label == NULL) spanned_error("Break outside loop", expr->span, "Break needs to be inside loop");
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            fprintf(code_stream, "    goto %s;", continue_label);
            if (return_var != NULL) fprintf(code_stream, "    %s = (%s) {};\n", return_var, gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics));
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            str object_var = gen_temp_c_name("object");
            if (!take_ref) fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, fa->object->resolved->type, type_generics, func_generics), object_var);
            else fprintf(code_stream, "    %s* %s;\n", gen_c_type_name(program, options, fa->object->resolved->type, type_generics, func_generics), object_var);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, fa->object, object_var, take_ref, continue_label, break_label);
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            if (take_ref) fprintf(code_stream, "&");
            if (fa->is_ref) {
                if (take_ref) fprintf(code_stream, "(**(%s**)%s).%s;\n", gen_c_type_name(program, options, fa->object->resolved->type->generics->generics.elements[0], type_generics, func_generics), object_var, fa->field->name);
                else fprintf(code_stream, "(*(%s*)%s).%s;\n", gen_c_type_name(program, options, fa->object->resolved->type->generics->generics.elements[0], type_generics, func_generics), object_var, fa->field->name);
            } else {
                if (take_ref) fprintf(code_stream, "(*%s).%s;\n", object_var, fa->field->name);
                else fprintf(code_stream, "%s.%s;\n", object_var, fa->field->name);
            }
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            str type = gen_c_type_name(program, options, assign->asignee->resolved->type, type_generics, func_generics);
            TypeValue* ty = replace_generic(program, options, assign->asignee->resolved->type, type_generics, func_generics, NULL, NULL);
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s* %s;\n", type, return_var);
            }
            str assignee = gen_temp_c_name("target");
            str value = gen_temp_c_name("value");
            fprintf(code_stream, "    %s* %s;\n", type, assignee);
            fprintf(code_stream, "    %s %s;\n", type, value);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, assign->asignee, assignee, true, continue_label, break_label);
            transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, assign->value, value, false, continue_label, break_label);
            if (!map_contains(ty->trait_impls, program->raii.copy_key)) {
                str dropped = gen_temp_c_name("dropped");
                fprintf(code_stream, "    %s %s = *%s;\n", type, dropped, assignee);
                transpile_drop(program, options, code_stream, type_generics, func_generics, assign->asignee->resolved->type, dropped);
            }
            fprintf(code_stream, "    *%s = %s;\n", assignee, value);
            if (return_var != NULL) fprintf(code_stream, "    %s = (%s) {};\n", return_var, gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics));
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
        } break;
        case EXPR_STRUCT_LITERAL: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            StructLiteral* slit = expr->expr;
            StrList fields = list_new(StrList);
            StrList field_names = list_new(StrList);
            map_foreach(slit->fields, str key, StructFieldLit* sfl, {
                UNUSED(key);
                if (sfl->value->type == EXPR_VARIABLE) {
                    list_append(&fields, gen_c_var_name(program, options, (Variable*)sfl->value->expr, type_generics, func_generics));
                } else {
                    str field = gen_temp_c_name("field");
                    fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, sfl->value->resolved->type, type_generics, func_generics), field);
                    transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, sfl->value, field, false, continue_label, break_label);
                    list_append(&fields, field);
                }
                list_append(&field_names, sfl->name->name);
            });
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            str c_ty = gen_c_type_name(program, options, slit->type, type_generics, func_generics);
            fprintf(code_stream, "(%s) { ", c_ty);
            list_foreach(&field_names, i, str name, {
                if (i > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, ".%s=%s", name, fields.elements[i]);
            });
            fprintf(code_stream, " };\n");
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=slit->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_C_INTRINSIC: {
            if (take_ref) { 
                return_var = gen_temp_c_name("takeref"); 
                fprintf(code_stream, "    %s %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), return_var);
            }
            fprintf(code_stream, "    ");
            if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
            CIntrinsic* ci = expr->expr;
            usize i = 0;
            usize len = strlen(ci->c_expr);
            char op = '\0';
            char mode = '\0';
            usize v_index = 0;
            usize t_index = 0;
            while (i < len) {
                char c = ci->c_expr[i++];
                if (op != '\0' && mode == '\0') {
                    mode = c;
                } else if (op != '\0') {
                    if (op == '@') {
                        TypeValue* tv = ci->type_bindings.elements[t_index];
                        i += ci->binding_sizes.elements[t_index + v_index];
                        t_index += 1;
                        if (mode == '.') {
                            fprint_resolved_typevalue(code_stream, tv, type_generics, func_generics, false);
                        } else if (mode == ':') {
                            fprint_resolved_typevalue(code_stream, tv, type_generics, func_generics, true);
                        } else if (mode == '!') {
                            str c_ty = gen_c_type_name(program, options, tv, type_generics, func_generics);
                            fprintf(code_stream, "%s", c_ty);
                        } else if (mode == '#') {
                            str tyname = to_str_writer(s, fprint_resolved_typevalue(s, tv, type_generics, func_generics, true));
                            fprintf(code_stream, "%lu", str_hash(tyname));
                        } else spanned_error("Invalid c intrinsic op", expr->span, "No such mode for intrinsci operator `%c%c`", op, mode);
                    } else if (op == '$') {
                        Variable* v = ci->var_bindings.elements[v_index];
                        i += ci->binding_sizes.elements[t_index + v_index];
                        v_index += 1;
                        if (mode == ':') {
                            if (v->box->name == NULL) panic("Compiler error: Add a check here");
                            str name = v->box->name;
                            fprintf(code_stream, "%s", name);
                        } else if (mode == '!') {
                            str c_var = gen_c_var_name(program, options, v, type_generics, func_generics);
                            fprintf(code_stream, "%s", c_var);
                        } else spanned_error("Invalid c intrinsic op", expr->span, "No such mode for intrinsic operator `%c%c`", op, mode);
                    } else spanned_error("Invalid c intrinsic op", expr->span, "No such intrinsic operator `%c`", op);
                    op = '\0';
                    mode = '\0';
                } else if (c == '@' || c == '$') {
                    op = c;
                } else {
                    fputc(c, code_stream);
                }
            }
            if (op != '\0') spanned_error("Invalid c intrinsic", expr->span, "intrinsic ended on operator: `%s`", ci->c_expr);
            fprintf(code_stream, ";\n");
            if (take_ref) fprintf(code_stream, "    %s = &%s;\n", original_return_var, return_var);
            if (take_ref || is_dropped) {
                DropItem di = { .c_var=return_var, .tv=expr->resolved->type };
                list_append(dil, di);
            }
        } break;
        case EXPR_DEREF: {
            Expression* inner = expr->expr;
            if (take_ref) {
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, inner, return_var, false, continue_label, break_label);
            } else {
                str ref = gen_temp_c_name("ref");
                fprintf(code_stream, "    %s* %s;\n", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), ref);
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, inner, ref, false, continue_label, break_label);
                fprintf(code_stream, "    ");
                if (return_var != NULL) fprintf(code_stream, "%s = ", return_var);
                fprintf(code_stream, "*%s;\n", ref);
            }
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            if (return_var != NULL && take_ref) {
                str ref = gen_temp_c_name("ref");
                fprintf(code_stream, "    %s %s;", gen_c_type_name(program, options, expr->resolved->type, type_generics, func_generics), ref);
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, inner, ref, true, continue_label, break_label);
                fprintf(code_stream, "    %s = &%s;\n", return_var, ref);
            } else {
                transpile_expression(program, options, dil, code_stream, func, type_generics, func_generics, inner, return_var, return_var != NULL, continue_label, break_label);
            }
        } break;
        default:
            unreachable("%s", ExprType__NAMES[expr->type]);
    }
}

void transpile_block(Program* program, CompilerOptions* options, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Block* block, str return_var, bool take_ref, str continue_label, str break_label) {
    DropItemList dil = list_new(DropItemList);
    list_foreach(&block->expressions, i, Expression* expr, {
        dil.length = 0;
        if (i == block->expressions.length - 1 && block->yield_last) {
            transpile_expression(program, options, &dil, code_stream, func, type_generics, func_generics, expr, return_var, take_ref, continue_label, break_label);
        } else {
            transpile_expression(program, options, &dil, code_stream, func, type_generics, func_generics, expr, NULL, false, continue_label, break_label);
        }
        list_foreach(&dil, i, DropItem item, {
            transpile_drop(program, options, code_stream, type_generics, func_generics, item.tv, item.c_var);
        });
    });
    list_foreach(&block->dropped, i, VarBox* var, {
        str c_name = gen_c_varbox_name(var);
        transpile_drop(program, options, code_stream, type_generics, func_generics, var->resolved->type, c_name);
    });
}

void transpile_function_generic_variant(Program* program, CompilerOptions* options, FILE* object_h_stream, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, str name) {    
    if (options->verbosity >= 4) log("transpiling function %s", name);
    reset_temp_context();
    if (options->tracelevel == 0) {
        program->tracegen.trace_this = false;
    } else if (options->tracelevel == 1) {
        Module* root = func->module;
        while (root->parent != NULL) root = root->parent;
        if (root == program->main_module) {
            program->tracegen.trace_this = true;
        } else {
            program->tracegen.trace_this = false;
        }
    } else {
        program->tracegen.trace_this = true;
    }
    
    str c_fn_name = gen_c_fn_name(program, options, func, type_generics, func_generics);
    str c_fn_default_name = gen_default_c_fn_name(program, options, func, type_generics, func_generics);
    str c_ret_ty = gen_c_type_name(program, options, func->return_type, type_generics, func_generics);
    str trace_f_name = to_str_writer(s, fprintf(s, "TRACE_%s", c_fn_default_name));
    fprintf(object_h_stream, "// %s\n", name);
    if (options->tracelevel > 0) {
        fprintf(object_h_stream, "extern %s %s;\n", program->tracegen.function_type_c_name, trace_f_name);
    }
    fprintf(object_h_stream, "%s %s(", c_ret_ty, c_fn_name);
    list_foreach(&func->args, i, Argument* arg, {
        str c_ty = gen_c_type_name(program, options, arg->type, type_generics, func_generics);
        str c_val = gen_c_var_name(program, options, arg->var, type_generics, func_generics);
        if (i > 0) fprintf(object_h_stream, ", ");
        fprintf(object_h_stream, "%s %s", c_ty, c_val);
    });
    if (func->is_variadic) {
        if (func->args.length > 0) fprintf(object_h_stream, ", ");
        fprintf(object_h_stream, "...");
    }
    fprintf(object_h_stream, ");\n");
    fprintf(object_h_stream, "\n");

    if (func->body != NULL) {
        fprintf(code_stream, "// %s\n", name);
    }
    if (options->tracelevel >= 0) {
        fprintf(code_stream, "%s %s = { .name=\"%s\", .full_name=\"%s\", .mangled_name=\"%s\", .loc={ .file=\"%s\", .line=%lld }, .is_method=1 };\n",
            program->tracegen.function_type_c_name, trace_f_name, 
            func->name->name, 
            name, 
            c_fn_name,
            func->name->span.left.file,
            func->name->span.left.line
        );
    }
    if (func->body != NULL) {
        fprintf(code_stream, "%s %s(", c_ret_ty, c_fn_name);
        list_foreach(&func->args, i, Argument* arg, {
            str c_ty = gen_c_type_name(program, options, arg->type, type_generics, func_generics);
            str c_val = gen_c_var_name(program, options, arg->var, type_generics, func_generics);
            if (i > 0) fprintf(code_stream, ", ");
            fprintf(code_stream, "%s %s", c_ty, c_val);
        });
        if (func->is_variadic) {
            if (func->args.length > 0) fprintf(code_stream, ", ");
            fprintf(code_stream, "...");
        }
        fprintf(code_stream, ") {\n");
        str return_var = NULL;
        str return_ty = gen_c_type_name(program, options, func->body->res, type_generics, func_generics);
        if (str_startswith(name, "::core::intrinsics::forget<")) { // compiler magic
            fprintf(code_stream, "    // compiler generated empty method body\n");
            fprintf(code_stream, "    return (%s) {};\n", return_ty);
        } else {
            if (func->body->yield_last) {
                return_var = gen_temp_c_name("return");
                fprintf(code_stream, "    %s %s;\n", return_ty, return_var);
            }
            transpile_block(program, options, code_stream, func, type_generics, func_generics, func->body, return_var, false,  NULL, NULL);
            if (func->body->yield_last) {
                fprintf(code_stream, "    return %s;\n", return_var);
            } else {
                fprintf(code_stream, "    return (%s) {};\n", return_ty);
            }
        }
        fprintf(code_stream, "}\n");
        fprintf(code_stream, "\n");
    }
}

void transpile_typedef_generic_variant(Program* program, CompilerOptions* options, FILE* type_h_stream, TypeValue* tv, str name) {
    if (tv->def->extern_ref != NULL) return;
    if (map_contains(program->t.type_done_map, name)) return;
    map_put(program->t.type_done_map, name, tv);
    list_foreach(&tv->def->flist, i, Identifier* name, {
        Field* f = map_get(tv->def->fields, name->name);
        TypeValue* field_tv = replace_generic(program, options, f->type, tv->generics, NULL, NULL, NULL);
        str field_key = to_str_writer(s, fprint_typevalue(s, field_tv));
        if (!map_contains(program->t.type_done_map, field_key)) {
            map_put(program->t.type_map, field_key, field_tv);
            transpile_typedef_generic_variant(program, options, type_h_stream, field_tv, field_key);
        }
    });
    if (options->verbosity >= 4) log("transpiling type %s", name);
    str c_name = gen_c_type_name(program, options, tv, NULL, NULL);
    fprintf(type_h_stream, "// %s\n", name);
    fprintf(type_h_stream, "%s", c_name);
    fprintf(type_h_stream, " {");
    if (map_size(tv->def->fields) > 0) fprintf(type_h_stream, "\n");
    else fprintf(type_h_stream, " ");
    list_foreach(&tv->def->flist, i, Identifier* name, {
        Field* f = map_get(tv->def->fields, name->name);
        str c_field_ty = gen_c_type_name(program, options, f->type, tv->generics, NULL);
        str c_field_name = name->name;
        fprintf(type_h_stream, "   %s %s;\n", c_field_ty, c_field_name);
    });
    fprintf(type_h_stream, "}");
    fprintf(type_h_stream, ";\n");
    fprintf(type_h_stream, "\n");
}

void transpile_static(Program* program, CompilerOptions* options, FILE* object_h_stream, FILE* code_stream, Global* s) {
    if (options->verbosity >= 4) {
        if (s->constant) panic("Expected static, got constant. This is a compiler error");
        log("transpiling static %s::%s", to_str_writer(stream, fprint_path(stream, s->module->path)), s->name->name);
    }
    bool is_extern = false;
    bool is_thread_local = false;
    list_foreach(&s->annotations, i, Annotation a, {
        str p = to_str_writer(s, fprint_path(s, a.path));
        if (str_eq(p, "extern")) {            
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[extern]`, found %s", AnnotationType__NAMES[a.type]);
            if (is_extern) spanned_error("Duplicate annotation flag", a.path->elements.elements[0]->span, "Flag `#[extern]` already set");
            is_extern = true;
        } else if (str_eq(p, "thread_local")) {            
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[thread_local]`, found %s", AnnotationType__NAMES[a.type]);
            if (is_thread_local) spanned_error("Duplicate annotation flag", a.path->elements.elements[0]->span, "Flag `#[thread_local]` already set");
            is_thread_local = true;
        }
    });
    str c_type = gen_c_type_name(program, options, s->type, NULL, NULL);
    str c_name = gen_c_static_name(s);
    fprintf(object_h_stream, "// static %s::%s\n", to_str_writer(stream, fprint_path(stream, s->module->path)), s->name->name);
    fprintf(object_h_stream, "extern ");
    if (is_thread_local) fprintf(object_h_stream, "__thread ");
    fprintf(object_h_stream, "%s %s;\n", c_type, c_name);
    fprintf(object_h_stream, "\n");
    if (!is_extern) {
        fprintf(code_stream, "// static %s::%s\n", to_str_writer(stream, fprint_path(stream, s->module->path)), s->name->name);
        if (is_thread_local) fprintf(code_stream, "__thread ");
        if (s->computed_value != NULL) {
            if (s->computed_value->type == STRING) {
                fprintf(code_stream, "%s %s = ", c_type, c_name);
                fprint_str_lit(code_stream, s->computed_value->string);
                fprintf(code_stream, ";\n");
            } else {
                fprintf(code_stream, "%s %s = %s;\n", c_type, c_name, s->computed_value->string);
            }
        } else {
            fprintf(code_stream, "%s %s;\n", c_type, c_name);
        }
        fprintf(code_stream, "\n");
    }
}

void transpile_statics_rec(Program* program, CompilerOptions* options, FILE* object_h_stream, FILE* code_stream, Module* mod) {
    map_foreach(mod->items, str name, ModuleItem* item, {
        UNUSED(name);
        if (item->origin != NULL) continue;
        switch (item->type) {
            case MIT_MODULE:
                transpile_statics_rec(program, options, object_h_stream, code_stream, item->item);
                break;
            case MIT_STATIC:
                transpile_static(program, options, object_h_stream, code_stream, item->item);
                break;
            default:
                break;
        }
    });
}

void queue_func(Program* program, CompilerOptions* options, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics) {
    str key = to_str_writer(s, {
        if (func->impl_type != NULL) {
            TypeValue* concrete = replace_generic(program, options, func->impl_type, type_generics, func_generics, NULL, NULL);
            fprint_path(s, func->module->path);
            fprint_typevalue(s, concrete);
        } else {
            fprint_path(s, func->module->path);
        }
        fprintf(s, "::%s", func->name->name);
        fprint_generic_values(s, func_generics);
        if (func->trait != NULL) {
            fprintf(s, " for %s::%s", to_str_writer(s, fprint_path(s, func->trait->module->path)), func->trait->name->name);
        }
    });
    if (map_contains(program->t.func_map, key)) return;
    if (options->verbosity >= 6) log("queueing function %s", key);
    map_put(program->t.func_map, key, func);
    QueuedFunc qf = { .key=key, .func=func, .type_generics=type_generics, .func_generics=func_generics };
    list_append(&program->t.func_queue, qf);
}

void queue_type(Program* program, CompilerOptions* options, TypeValue* tv) {
    if (tv->def->extern_ref != NULL) return;
    str key = to_str_writer(s, fprint_typevalue(s, tv));
    if (map_contains(program->t.type_map, key)) return;
    if (options->verbosity >= 6) log("queueing type %s", key);
    map_put(program->t.type_map, key, tv);
    QueuedType qt = { .type=tv, .key=key };
    list_append(&program->t.type_queue, qt);
}

void transpile_to_c(Program* program, CompilerOptions* options, FILE* type_h_stream, FILE* object_h_stream, FILE* code_stream, str base_name) {
    program->t.type_map = map_new();
    program->t.type_done_map = map_new();
    program->t.func_map = map_new();
    program->t.type_queue = list_new(QTList);
    program->t.func_queue = list_new(QFList);
    program->tracegen.top_frame_c_name = gen_c_var_name(program, options, program->tracegen.top_frame, NULL, NULL);
    program->tracegen.frame_type_c_name = gen_c_type_name(program, options, program->tracegen.frame_type, NULL, NULL);
    program->tracegen.function_type_c_name = gen_c_type_name(program, options, program->tracegen.function_type, NULL, NULL);
    PTR_DEF = resolve_item_raw(program, options, program->main_module, gen_path("::core::types::ptr"), MIT_STRUCT, NULL)->item;

    usize len = strlen(base_name);
    str base_name_upper = malloc(len+1);
    usize i = 0;
    while (i < len) {
        base_name_upper[i] = toupper(base_name[i]);
        i += 1;
    }
    base_name_upper[len] = '\0';

    fprintf(type_h_stream, "/*\n *  File generated by kommando compiler.\n *  This file contains the type definitions.\n */\n\n#ifndef %s_T_H\n#define %s_T_H\n\n", base_name_upper, base_name_upper);
    fprintf(object_h_stream, "#/*\n *  File generated by kommando compiler.\n *  This file comtains all symbol definitions and some macros.\n */\n\n#ifndef %s_O_H\n#define %s_O_H\n\n", base_name_upper, base_name_upper);
    fprintf(code_stream, "/*\n *  File generated by kommando compiler.\n *  This is the omplementation file.\n */\n\n");

    if (options->c_headers.length > 0) {
        list_foreach(&options->c_headers, i, str header, {
            fprintf(type_h_stream, "#include <%s>\n", header);
            fprintf(object_h_stream, "#include <%s>\n", header);
        });
        fprintf(type_h_stream, "\n");
        fprintf(object_h_stream, "\n");
    }

    fprintf(object_h_stream, "#include \"%s_t.h\"\n", base_name);
    fprintf(object_h_stream, "\n");
    fprintf(code_stream, "#include \"%s_t.h\"\n", base_name);
    fprintf(code_stream, "#include \"%s_o.h\"\n", base_name);
    fprintf(code_stream, "\n");

    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "PASS" ANSI_RESET_SEQUENCE, "Collecting visible symbols");

    ModuleItem* main_func_item = map_get(program->main_module->items, "main");
    FuncDef* main_func = main_func_item->item;
    queue_func(program, options, main_func, NULL, NULL);

    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "PASS" ANSI_RESET_SEQUENCE, "Transpiling statics");
    map_foreach(program->packages, str name, Module* mod, {
        UNUSED(name);
        transpile_statics_rec(program, options, object_h_stream, code_stream, mod);
    });

    if (options->verbosity >= 2) info(ANSI(ANSI_BOLD, ANSI_YELLO_FG) "PASS" ANSI_RESET_SEQUENCE, "Transpiling queued items");
    usize func_index = 0;
    usize type_index = 0;
    while (true) {
        if (type_index < program->t.type_queue.length) {
            QueuedType qt = program->t.type_queue.elements[type_index];
            transpile_typedef_generic_variant(program, options, type_h_stream, qt.type, qt.key);
            type_index += 1;
            continue;
        }
        if (func_index < program->t.func_queue.length) {
            QueuedFunc qf = program->t.func_queue.elements[func_index];
            transpile_function_generic_variant(program, options, object_h_stream, code_stream, qf.func, qf.type_generics, qf.func_generics, qf.key);
            func_index += 1;
            continue;
        }
        break;
    }

    fprintf(type_h_stream, "#endif\n");
    fprintf(object_h_stream, "#endif\n");

    if (!options->raw) {
        str main_c_name = gen_c_fn_name(program, options, main_func, NULL, NULL);
        fprintf(code_stream, "// c main entrypoint\n");
        fprintf(code_stream, "int main(int argc, char** argv) {\n");
        fprintf(code_stream, "    __global_argc = argc;\n");
        fprintf(code_stream, "    __global_argv = argv;\n");
        str local_frame_name = NULL;
        if (options->tracelevel > 0) {
            local_frame_name = gen_temp_c_name("local_frame");
            fprintf(code_stream, "    %s %s = { .parent=%s, .call=&TRACE_%s, .loc={ .file=0, .line=0 }, .callflags=0b1 };\n", program->tracegen.frame_type_c_name, local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(program, options, main_func, NULL, NULL));
            fprintf(code_stream, "    %s = &%s;\n", program->tracegen.top_frame_c_name, local_frame_name);
        }
        fprintf(code_stream, "    %s();\n", main_c_name);
        if (options->tracelevel > 0) {
            fprintf(code_stream, "    %s = %s.parent;\n", program->tracegen.top_frame_c_name, local_frame_name);
        }
        fprintf(code_stream, "    return 0;\n");
        fprintf(code_stream, "}\n");
    }
}
