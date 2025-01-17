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
#include "lib/str.h"
LIB
#include "transpiler_c.h"
#include "ast.h"
#include "module.h"
#include "parser.h"
#include "resolver.h"
#include "token.h"

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

static str gen_c_type_name_inner(TypeValue* tv, GenericValues* type_generics, GenericValues* func_generics, bool toplevel) {
    TypeDef* ty = tv->def;
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
            return gen_c_type_name_inner(concrete, func_generics, type_generics, toplevel);
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
                fprintf(stream, "%s", gen_c_type_name_inner(generic_tv, func_generics, type_generics, false));
            });
            fprintf(stream, ">");
        }
    });
    u32 hash = str_hash(key);
    return to_str_writer(stream, fprintf(stream,"struct %s%lx", ty->name->name, hash));
}
str gen_c_type_name(TypeValue* tv, GenericValues* type_generics, GenericValues* func_generics) {
    return gen_c_type_name_inner(tv, type_generics, func_generics, true);
}

str gen_default_c_fn_name(FuncDef* def, GenericValues* type_generics, GenericValues* func_generics) {
    str data = to_str_writer(s, {
        fprintf(s, "%p%s", def, tfvals_to_key(type_generics, func_generics));
    });
    u32 hash = str_hash(data);
    if (def->impl_type == NULL) {
        return to_str_writer(s, fprintf(s,"%s%lx", def->name->name, hash));
    } else {
        return to_str_writer(s, fprintf(s,"%s_%s%lx", def->impl_type->def->name->name, def->name->name, hash));
    }
}

str gen_c_fn_name(FuncDef* def, GenericValues* type_generics, GenericValues* func_generics) {
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
    return gen_default_c_fn_name(def, type_generics, func_generics);
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
        }
        if (str_eq(p, "no_mangle")) {
            if (state == 1) spanned_error("Invalid annotation", a.path->elements.elements[0]->span, "Cannot both define symbol name and no_mangle");
            if (state == 2) spanned_error("Duplicate annotation", a.path->elements.elements[0]->span, "no_mangle already set");
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[no_mangle]`, found %s", AnnotationType__NAMES[a.type]);
            name = s->name->name;
            state = 2;
        }
    });
    if (name == NULL) {
        name = to_str_writer(stream, { 
            fprintf(stream, "%s%llx", s->name->name, (usize)s);
        });
    }
    return name;
}

str gen_c_var_name(Variable* v, GenericValues* type_generics, GenericValues* func_generics) {
    if (v->global_ref != NULL) {
        if (v->global_ref->constant) {
            if (v->global_ref->computed_value->type == STRING) {
                return to_str_writer(s, fprintf(s, "\"%s\"", v->global_ref->computed_value->string));
            } else {
                return v->global_ref->computed_value->string;
            }
        } 
        return gen_c_static_name(v->global_ref);
    }
    if (v->box->name != NULL) {
        return to_str_writer(stream, fprintf(stream,"%s%llx", v->box->name, (usize)v->box));
    } else {
        switch (v->box->mi->type) {
            case MIT_FUNCTION: {
                FuncDef* func = v->box->mi->item;
                GenericValues* func_gens = v->values;
                if (func_gens != NULL) {
                    func_gens = expand_generics(func_gens, type_generics, func_generics);
                }
                return gen_c_fn_name(func, NULL, func_gens);
            } break;
            default:
                unreachable();
        }
    }
}

str gen_temp_c_name(str name) {
    return to_str_writer(stream, fprintf(stream, "%s%lxt", name, random()));
}

void fprint_indent(FILE* file, usize indent) {
    for (usize i = 0;i < indent;i++) {
        fprintf(file, "    ");
    }
}

void transpile_block(Program* program, CompilerOptions* options, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Block* block, bool use_result, usize indent);
void transpile_expression(Program* program, CompilerOptions* options, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Expression* expr, bool use_result, usize indent) {
    //log("'%s' %s", ExprType__NAMES[expr->type], to_str_writer(s, fprint_span(s, &expr->span)));
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            fprintf(code_stream, "(");
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, op->lhs, true, indent + 1);
            fprintf(code_stream, " %s ", op->op);
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, op->rhs, true, indent + 1);
            fprintf(code_stream, ")");
        } break;
        case EXPR_BIN_OP_ASSIGN: {
            BinOp* op = expr->expr;
            str temp = gen_temp_c_name("temp");
            str ty = gen_c_type_name(op->lhs->resolved->type, type_generics, func_generics);
            fprintf(code_stream, "({ ");
            fprintf(code_stream, "%s* %s = &(", ty, temp);
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, op->lhs, true, indent + 1);
            fprintf(code_stream, "); *%s = *%s %s ", temp, temp, op->op);
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, op->rhs, true, indent + 1);
            if (use_result) {
                fprintf(code_stream, "; (%s){} })", gen_c_type_name(expr->resolved->type, type_generics, func_generics));
            } else {
                fprintf(code_stream, "; })");
            }
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncDef* fd = fc->def;
            GenericValues* fc_generics = fc->generics;
            if (fc->generics != NULL) {
                fc_generics = expand_generics(fc_generics, type_generics, func_generics);
            }
            str c_fn_name = gen_c_fn_name(fd, NULL, fc_generics);
            Module* root = fd->module;
            while (root->parent != NULL) { root = root->parent; }
            if (program->tracegen.trace_this && !fd->untraced) {
                str temp = gen_temp_c_name("result");
                str result_type = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
                StrList argnames = list_new(StrList);
                fprintf(code_stream, "({\n");
                list_foreach(&fc->arguments, i, Expression* arg, {
                    str argname = gen_temp_c_name("arg");
                    fprint_indent(code_stream, indent);
                    fprintf(code_stream, "%s %s = ", gen_c_type_name(arg->resolved->type, type_generics, func_generics), argname);
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, arg, true, indent + 1);
                    fprintf(code_stream, ";\n");
                    list_append(&argnames, argname);
                });
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s %s = { .parent=%s, .call=&TRACE_%s, .file=\"%s\", .line=%lld, .callflags=", program->tracegen.frame_type_c_name, program->tracegen.local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(fd, NULL, fc_generics), fc->name->elements.elements[0]->span.left.file, fc->name->elements.elements[0]->span.left.line);
                if (fd->body == NULL) fprintf(code_stream, "0b110");
                else if (func->module == program->main_module && root != program->main_module && options->tracelevel < 2) fprintf(code_stream, "0b100");
                else fprintf(code_stream, "0b0");
                fprintf(code_stream, " };\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s = &%s;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
                fprint_indent(code_stream, indent);
                if (use_result) fprintf(code_stream, "%s %s = %s(", result_type, temp, c_fn_name);
                else fprintf(code_stream, "%s(", c_fn_name);
                list_foreach(&argnames, i, str arg, {
                    if (i > 0) fprintf(code_stream, ", ");
                    fprintf(code_stream, "%s", arg);
                });
                fprintf(code_stream, ");\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s = %s.parent;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
                fprint_indent(code_stream, indent);
                if (use_result) fprintf(code_stream, "%s;\n", temp);
                fprint_indent(code_stream, indent-1);
                fprintf(code_stream, "})");
            } else {
                fprintf(code_stream, "%s(", c_fn_name);
                list_foreach(&fc->arguments, i, Expression* arg, {
                    if (i > 0) fprintf(code_stream, ", ");
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, arg, true, indent + 1);
                });
                fprintf(code_stream, ")");
            }
        } break;
        case EXPR_METHOD_CALL: {
            MethodCall* call = expr->expr;
            FuncDef* fd = call->def->func;
            GenericValues* call_generics = expand_generics(call->generics, type_generics, func_generics);
            GenericValues* type_call_generics = expand_generics(call->impl_vals, type_generics, func_generics);
            str c_fn_name = gen_c_fn_name(fd, type_call_generics, call_generics);
            Module* root = fd->module;
            while (root->parent != NULL) { root = root->parent; }
            if (program->tracegen.trace_this && !fd->untraced) {
                str temp = gen_temp_c_name("result");
                str result_type = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
                StrList argnames = list_new(StrList);
                fprintf(code_stream, "({\n");
                fprint_indent(code_stream, indent);
                str thisname = gen_temp_c_name("this");
                fprintf(code_stream, "%s %s = ", gen_c_type_name(call->object->resolved->type, type_generics, func_generics), thisname);
                transpile_expression(program, options, code_stream, func, type_generics, func_generics, call->object, true, indent + 1);
                fprintf(code_stream, ";\n");
                list_append(&argnames, thisname);
                list_foreach(&call->arguments, i, Expression* arg, {
                    str argname = gen_temp_c_name("arg");
                    fprint_indent(code_stream, indent);
                    fprintf(code_stream, "%s %s = ", gen_c_type_name(arg->resolved->type, type_generics, func_generics), argname);
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, arg, true, indent + 1);
                    fprintf(code_stream, ";\n");
                    list_append(&argnames, argname);
                });
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s %s = { .parent=%s, .call=&TRACE_%s, .file=\"%s\", .line=%lld, .callflags=", program->tracegen.frame_type_c_name, program->tracegen.local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(fd, type_call_generics, call_generics), call->name->span.left.file, call->name->span.left.line);
                if (fd->body == NULL) fprintf(code_stream, "0b110");
                else if (func->module == program->main_module && root != program->main_module && options->tracelevel < 2) fprintf(code_stream, "0b100");
                else fprintf(code_stream, "0b0");
                fprintf(code_stream, " };\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s = &%s;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
                fprint_indent(code_stream, indent);
                if (use_result) fprintf(code_stream, "%s %s = %s(", result_type, temp, c_fn_name);
                else fprintf(code_stream, "%s(", c_fn_name);
                list_foreach(&argnames, i, str arg, {
                    if (i > 0) fprintf(code_stream, ", ");
                    fprintf(code_stream, "%s", arg);
                });
                fprintf(code_stream, ");\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s = %s.parent;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
                fprint_indent(code_stream, indent);
                if (use_result) fprintf(code_stream, "%s;\n", temp);
                fprint_indent(code_stream, indent-1);
                fprintf(code_stream, "})");
            } else {
                fprintf(code_stream, "%s(", c_fn_name);
                transpile_expression(program, options, code_stream, func, type_generics, func_generics, call->object, true, indent + 1);
                if (call->arguments.length > 0) fprintf(code_stream, ", ");
                list_foreach(&call->arguments, i, Expression* arg, {
                    if (i > 0) fprintf(code_stream, ", ");
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, arg, true, indent + 1);
                });
                fprintf(code_stream, ")");
            }
        } break;
        case EXPR_STATIC_METHOD_CALL: {
            StaticMethodCall* call = expr->expr;
            FuncDef* fd = call->def->func;
            GenericValues* call_generics = expand_generics(call->generics, type_generics, func_generics);
            GenericValues* type_call_generics = expand_generics(call->impl_vals, type_generics, func_generics);
            str c_fn_name = gen_c_fn_name(fd, type_call_generics, call_generics);
            Module* root = fd->module;
            while (root->parent != NULL) { root = root->parent; }
            if (program->tracegen.trace_this && !fd->untraced) {
                str temp = gen_temp_c_name("result");
                str result_type = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
                StrList argnames = list_new(StrList);
                fprintf(code_stream, "({\n");
                list_foreach(&call->arguments, i, Expression* arg, {
                    str argname = gen_temp_c_name("arg");
                    fprint_indent(code_stream, indent);
                    fprintf(code_stream, "%s %s = ", gen_c_type_name(arg->resolved->type, type_generics, func_generics), argname);
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, arg, true, indent + 1);
                    fprintf(code_stream, ";\n");
                    list_append(&argnames, argname);
                });
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s %s = { .parent=%s, .call=&TRACE_%s, .file=\"%s\", .line=%lld, .callflags=", program->tracegen.frame_type_c_name, program->tracegen.local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(fd, type_call_generics, call_generics), call->name->span.left.file, call->name->span.left.line);
                if (fd->body == NULL) fprintf(code_stream, "0b110");
                else if (func->module == program->main_module && root != program->main_module && options->tracelevel < 2) fprintf(code_stream, "0b100");
                else fprintf(code_stream, "0b0");
                fprintf(code_stream, " };\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s = &%s;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
                fprint_indent(code_stream, indent);
                if (use_result) fprintf(code_stream, "%s %s = %s(", result_type, temp, c_fn_name);
                else fprintf(code_stream, "%s(", c_fn_name);
                list_foreach(&argnames, i, str arg, {
                    if (i > 0) fprintf(code_stream, ", ");
                    fprintf(code_stream, "%s", arg);
                });
                fprintf(code_stream, ");\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s = %s.parent;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
                fprint_indent(code_stream, indent);
                if (use_result) fprintf(code_stream, "%s;\n", temp);
                fprint_indent(code_stream, indent-1);
                fprintf(code_stream, "})");
            } else {
                fprintf(code_stream, "%s(", c_fn_name);
                list_foreach(&call->arguments, i, Expression* arg, {
                    if (i > 0) fprintf(code_stream, ", ");
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, arg, true, indent + 1);
                });
                fprintf(code_stream, ")");
            }
        } break;
        case EXPR_DYN_RAW_CALL: {
            DynRawCall* fc = expr->expr;
            fprintf(code_stream, "(");
            str c_ret = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            fprintf(code_stream, "(%s(*)())", c_ret);
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, fc->callee, true, indent + 1);
            fprintf(code_stream, ")");
            fprintf(code_stream, "(");
            list_foreach(&fc->args, i, Expression* arg, {
                if (i > 0) fprintf(code_stream, ", ");
                transpile_expression(program, options, code_stream, func, type_generics, func_generics, arg, true, indent + 1);
            });
            fprintf(code_stream, ")");
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING:
                    fprint_token(code_stream, lit);
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
        } break;
        case EXPR_BLOCK: {
            transpile_block(program, options, code_stream, func, type_generics, func_generics, expr->expr, use_result, 1);
        } break;
        case EXPR_VARIABLE: {
            Variable* v = expr->expr;
            fprintf(code_stream, "%s", gen_c_var_name(v, type_generics, func_generics));
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;
            str c_ty = gen_c_type_name(let->type, type_generics, func_generics);
            str c_name = gen_c_var_name(let->var, type_generics, func_generics);
            if (use_result) {
                fprintf(code_stream, "({ ");
            }
            fprintf(code_stream, "%s %s = ", c_ty, c_name);
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, let->value, true, indent + 1);
            if (use_result) {
                fprintf(code_stream, "; (%s){} })", gen_c_type_name(expr->resolved->type, type_generics, func_generics));
            }
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            if (use_result) {
                str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
                str ret_var = gen_temp_c_name("temp");
                fprintf(code_stream, "({\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s %s;\n", ret_ty, ret_var);
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "if (");
                transpile_expression(program, options, code_stream, func,  type_generics, func_generics, cond->cond, true, indent);
                fprintf(code_stream, ") %s = ", ret_var);
                transpile_block(program, options, code_stream, func,  type_generics, func_generics, cond->then, true, indent);
                fprintf(code_stream, ";\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "else %s = ", ret_var);
                if (cond->otherwise != NULL) {
                    transpile_block(program, options, code_stream, func,  type_generics, func_generics, cond->otherwise, true, indent);
                } else {
                    fprintf(code_stream, "(%s) {}", ret_ty);
                }
                fprintf(code_stream, ";\n");
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "%s;\n", ret_var);
                fprint_indent(code_stream, indent-1);
                fprintf(code_stream, "})");
            } else {
                fprintf(code_stream, "if (");
                transpile_expression(program, options, code_stream, func,  type_generics, func_generics, cond->cond, true, indent);
                fprintf(code_stream, ") ");
                transpile_block(program, options, code_stream, func,  type_generics, func_generics, cond->then, false, indent);
                if (cond->otherwise != NULL) {
                    fprintf(code_stream, ";\n");
                    fprint_indent(code_stream, indent);
                    fprintf(code_stream, "else ");
                    transpile_block(program, options, code_stream, func,  type_generics, func_generics, cond->otherwise, false, indent);
                }
            }
        } break;
        case EXPR_WHILE_LOOP: {
            WhileLoop* wl = expr->expr;
            str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            if (use_result) { 
                fprintf(code_stream, "({\n");
                fprint_indent(code_stream, indent + 1);
                fprintf(code_stream, "while (");
                transpile_expression(program, options, code_stream, func, type_generics, func_generics, wl->cond, true, indent + 1);
                fprintf(code_stream, ") ");
                transpile_block(program, options, code_stream, func,  type_generics, func_generics, wl->body, false, indent + 1);
                fprintf(code_stream, ";\n");
                fprint_indent(code_stream, indent + 1);
                fprintf(code_stream, "(%s) {};\n", ret_ty);
                fprint_indent(code_stream, indent);
                fprintf(code_stream, "})");
            } else {
                fprintf(code_stream, "while (");
                transpile_expression(program, options, code_stream, func, type_generics, func_generics, wl->cond, true, indent);
                fprintf(code_stream, ") ");
                transpile_block(program, options, code_stream, func, type_generics, func_generics, wl->body, false, indent);
            }
        } break;
        case EXPR_RETURN: {
            str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            if (use_result) {
                fprintf(code_stream, "({ ");
                if (expr->expr == NULL) {
                    fprintf(code_stream, "return (%s) {};", ret_ty);
                } else {
                    fprintf(code_stream, "return ");
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, expr->expr, true, indent + 1);
                }
                fprintf(code_stream, "; (%s) {}; })", ret_ty);
            } else {
                if (expr->expr == NULL) {
                    fprintf(code_stream, "return (%s) {}", ret_ty);
                } else {
                    fprintf(code_stream, "return ");
                    transpile_expression(program, options, code_stream, func, type_generics, func_generics, expr->expr, true, indent);
                }
            }
        } break;
        case EXPR_BREAK: {
            if (expr->expr != NULL) todo("EXPR_BREAK with expr != NULL");
            if (use_result) {
                str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
                fprintf(code_stream, "({ break; (%s) {}; })", ret_ty);
            } else {
                fprintf(code_stream, "break");
            }
        } break;
        case EXPR_CONTINUE: {
            if (use_result) {
                str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
                fprintf(code_stream, "({ continue; (%s) {}; })", ret_ty);
            } else {
                fprintf(code_stream, "continue");
            }
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, fa->object, true, indent);
            fprintf(code_stream, ".%s", fa->field->name);
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            if (use_result) {
                fprintf(code_stream, "({ ");
            }
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, assign->asignee, true, indent + 1);
            fprintf(code_stream, " = ");
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, assign->value, true, indent + 1);
             if (use_result) {
                fprintf(code_stream, "; (%s){} })", gen_c_type_name(expr->resolved->type, type_generics, func_generics));
            }
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            str c_ty = gen_c_type_name(slit->type, type_generics, func_generics);
            fprintf(code_stream, "(%s) { ", c_ty);
            usize i = 0;
            map_foreach(slit->fields, str key, StructFieldLit* sfl, {
                UNUSED(key);
                if (i++ > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, ".%s=", sfl->name->name);
                transpile_expression(program, options, code_stream, func, type_generics, func_generics, sfl->value, true, indent + 1);
            });
            fprintf(code_stream, " }");
        } break;
        case EXPR_C_INTRINSIC: {
            CIntrinsic* ci = expr->expr;
            usize i = 0;
            usize len = strlen(ci->c_expr);
            char op = '\0';
            char mode = '\0';
            while (i < len) {
                char c = ci->c_expr[i++];
                if (op != '\0' && mode == '\0') {
                    mode = c;
                } else if (op != '\0') {
                    if (op == '@') {
                        str key = malloc(2);
                        key[0] = c;
                        key[1] = '\0';
                        TypeValue* tv = map_get(ci->type_bindings, key);
                        if (tv == NULL) spanned_error("Invalid c intrinsic", expr->span, "intrinsic does not bind type @%s: `%s`", key, ci->c_expr);
                        if (mode == '.') {
                            fprint_resolved_typevalue(code_stream, tv, type_generics, func_generics, false);
                        } else if (mode == ':') {
                            fprint_resolved_typevalue(code_stream, tv, type_generics, func_generics, true);
                        } else if (mode == '!') {
                            str c_ty = gen_c_type_name(tv, type_generics, func_generics);
                            fprintf(code_stream, "%s", c_ty);
                        } else if (mode == '#') {
                            str tyname = to_str_writer(s, fprint_resolved_typevalue(s, tv, type_generics, func_generics, true));
                            fprintf(code_stream, "%lu", str_hash(tyname));
                        } else spanned_error("Invalid c intrinsic op", expr->span, "No such mode for intrinsci operator `%c%c`", op, mode);
                    } else if (op == '$') {
                        str key = malloc(2);
                        key[0] = c;
                        key[1] = '\0';
                        Variable* v = map_get(ci->var_bindings, key);
                        if (v == NULL) spanned_error("Invalid c intrinsic", expr->span, "intrinsic does not bind variale $%s: `%s`", key, ci->c_expr);
                        if (mode == ':') {
                            if (v->box->name == NULL) panic("Compiler error: Add a check here");
                            str name = v->box->name;
                            fprintf(code_stream, "%s", name);
                        } else if (mode == '!') {
                            str c_var = gen_c_var_name(v, type_generics, func_generics);
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
        } break;
        case EXPR_DEREF: {
            Expression* inner = expr->expr;
            fprintf(code_stream, "(*(%s*)(", gen_c_type_name(expr->resolved->type, type_generics, func_generics));
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, inner, true, indent + 1);
            fprintf(code_stream, "))");
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            fprintf(code_stream, "&(");
            transpile_expression(program, options, code_stream, func, type_generics, func_generics, inner, true, indent + 1);
            fprintf(code_stream, ")");
        } break;
        default:
            unreachable("%s", ExprType__NAMES[expr->type]);
    }
}

void transpile_block(Program* program, CompilerOptions* options, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Block* block, bool use_result, usize indent) {
    if (block->expressions.length == 1 && (block->yield_last || !use_result)) {
        Expression* expr = block->expressions.elements[0];
        transpile_expression(program, options, code_stream, func, type_generics, func_generics, expr, block->yield_last && use_result, indent + 1);
        return;
    }
    fprintf(code_stream, "({\n");
    list_foreach(&block->expressions, i, Expression* expr, {
        fprint_indent(code_stream, indent + 1);
        transpile_expression(program, options, code_stream, func, type_generics, func_generics, expr, i == block->expressions.length-1 && block->yield_last && use_result, indent + 1);
        fprintf(code_stream, ";\n");
    });
    if (!block->yield_last && use_result) {
        fprint_indent(code_stream, indent + 1);
        fprintf(code_stream, "(%s) {};\n", gen_c_type_name(block->res, type_generics, func_generics));
    }
    fprint_indent(code_stream, indent);
    fprintf(code_stream, "})");
}

static TypeValue* find_contexted(GenericValues* instance) {
    if (instance == NULL) return NULL;
    for (usize i = 0;i < instance->generics.length;i++) {
        TypeValue* tv = instance->generics.elements[i];
        if (tv->ctx != NULL) {
            return tv;
        }
        GenericValues* v = tv->generics;
        if (v == NULL) continue;
        tv = find_contexted(v);
        if (tv != NULL) return tv;
    }
    return NULL;
}

static bool is_fully_defined(GenericValues* instance) {
    if (instance == NULL) return true;
    for (usize i = 0;i < instance->generics.length;i++) {
        TypeValue* t = instance->generics.elements[i];
        if (t->def->module == NULL) {
            return false;
        }
        if (!is_fully_defined(t->generics)) return false;
    }
    return true;
}

bool monomorphize(GenericValues* type_instance, GenericValues* func_instance, GenericKeys* instance_host, FuncDef* ctx) {
    if (type_instance != NULL) {
        TypeValue* type_contexted = find_contexted(type_instance);
        if (type_contexted == NULL) {
            if (!is_fully_defined(type_instance)) spanned_error("Not fully defined", type_instance->span, "This is probably a compiler error. For now just supply more generic hints. Defined @ %s", to_str_writer(s, fprint_span(s, &instance_host->span)));
            goto type_done;
        }
        map_foreach(type_contexted->ctx->generic_uses, str key, GenericUse* use, {
            if (ctx != NULL && ctx != use->in_func && false) {
                //log("skipping %s in %s as we are inside %s", key, use->in_func->name->name, ctx->name->name);
                continue;
            } else {
                //if (ctx != NULL) log("key %s inside %s", key, ctx->name->name);
                //else log("key %s outside", key);
            }
            UNUSED(key);
            GenericValues* expanded = expand_generics(type_instance, use->type_context, use->func_context);
            if (expanded == NULL) continue;
            str expanded_key = tfvals_to_key(expanded, func_instance);
            if (!map_contains(instance_host->generic_uses, expanded_key)) {
                GenericUse* new_use = malloc(sizeof(GenericUse));
                new_use->type_context = expanded;
                new_use->func_context = func_instance;
                new_use->in_func = ctx;
                map_put(instance_host->generic_uses, expanded_key, new_use);
                // we are iterating over this but in this case we can safely append, even if realloc
                list_append(&instance_host->generic_use_keys, expanded_key);
            }
        });
        return false;
    }
    type_done: {}
    if (func_instance != NULL) {
        TypeValue* func_contexted = find_contexted(func_instance);
        if (func_contexted == NULL) {
            if (!is_fully_defined(func_instance)) spanned_error("Not fully defined", func_instance->span, "This is probably a compiler error. For now just supply more generic hints.");
            goto func_done;
        }
        map_foreach(func_contexted->ctx->generic_uses, str key, GenericUse* use, {
            if (ctx != NULL && ctx != use->in_func) {
                //log("switched %s from %s to %s", key, use->in_func->name->name, ctx->name->name);
            } else {
                //if (ctx != NULL) log("key %s inside %s", key, ctx->name->name);
                //else log("key %s outside", key);
            }
            UNUSED(key);
            GenericValues* expanded = expand_generics(func_instance, use->type_context, use->func_context);
            if (expanded == NULL) continue;
            str expanded_key = tfvals_to_key(type_instance, expanded);
            if (!map_contains(instance_host->generic_uses, expanded_key)) {
                GenericUse* new_use = malloc(sizeof(GenericUse));
                new_use->type_context = type_instance;
                new_use->func_context = expanded;
                new_use->in_func = use->in_func;
                map_put(instance_host->generic_uses, expanded_key, new_use);
                // we are iterating over this but in this case we can safely append, even if realloc
                list_append(&instance_host->generic_use_keys, expanded_key);
            }
        });
        return false;
    }
    func_done: {}
    return true;
}


void transpile_function_generic_variant(Program* program, CompilerOptions* options, FILE* header_stream, FILE* code_stream, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics) {    
    str modpath = to_str_writer(s, fprint_path(s, func->module->path));
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
    
    str c_fn_name = gen_c_fn_name(func, type_generics, func_generics);
    str c_fn_default_name = gen_default_c_fn_name(func, type_generics, func_generics);
    str c_ret_ty = gen_c_type_name(func->return_type, type_generics, func_generics);
    str trace_f_name = to_str_writer(s, fprintf(s, "TRACE_%s", c_fn_default_name));
    if (func->impl_type == NULL) fprintf(header_stream, "// %s::%s%s%s\n", modpath, func->name->name, gvals_to_c_key(type_generics), gvals_to_c_key(func_generics));
    else fprintf(header_stream, "// %s%s::%s%s\n", modpath, gvals_to_c_key(type_generics), func->name->name, gvals_to_c_key(func_generics));
    if (options->tracelevel > 0) {
        fprintf(header_stream, "extern %s %s;\n", program->tracegen.function_type_c_name, trace_f_name);
    }
    fprintf(header_stream, "%s %s(", c_ret_ty, c_fn_name);
    list_foreach(&func->args, i, Argument* arg, {
        str c_ty = gen_c_type_name(arg->type, type_generics, func_generics);
        str c_val = gen_c_var_name(arg->var, type_generics, func_generics);
        if (i > 0) fprintf(header_stream, ", ");
        fprintf(header_stream, "%s %s", c_ty, c_val);
    });
    if (func->is_variadic) {
        if (func->args.length > 0) fprintf(header_stream, ", ");
        fprintf(header_stream, "...");
    }
    fprintf(header_stream, ");\n");
    fprintf(header_stream, "\n");

    if (func->body != NULL) {
        if (func->impl_type == NULL) fprintf(code_stream, "// %s::%s%s%s\n", modpath, func->name->name, gvals_to_c_key(type_generics), gvals_to_c_key(func_generics));
        else fprintf(code_stream, "// %s%s::%s%s\n", to_str_writer(s, fprint_td_path(s, func->impl_type->def)), gvals_to_c_key(type_generics), func->name->name, gvals_to_c_key(func_generics));
    }
    if (options->tracelevel >= 0) {
        if (func->impl_type == NULL) fprintf(code_stream, "%s %s = { .name=\"%s\", .full_name=\"%s::%s%s%s\", .mangled_name=\"%s\", .file=\"%s\", .line=%lld, .is_method=1 };\n",
            program->tracegen.function_type_c_name, trace_f_name, 
            func->name->name, 
            modpath, func->name->name, gvals_to_c_key(type_generics), gvals_to_c_key(func_generics), 
            c_fn_name,
            func->name->span.left.file,
            func->name->span.left.line);
        else fprintf(code_stream, "%s %s = { .name=\"%s\", .full_name=\"%s%s::%s%s\", .mangled_name=\"%s\", .file=\"%s\", .line=%lld, .is_method=1 };\n",
            program->tracegen.function_type_c_name, trace_f_name, 
            func->name->name, 
            modpath, gvals_to_c_key(type_generics), func->name->name, gvals_to_c_key(func_generics), 
            c_fn_name,
            func->name->span.left.file,
            func->name->span.left.line);
    }
    if (func->body != NULL) {
        fprintf(code_stream, "%s %s(", c_ret_ty, c_fn_name);
        list_foreach(&func->args, i, Argument* arg, {
            str c_ty = gen_c_type_name(arg->type, type_generics, func_generics);
            str c_val = gen_c_var_name(arg->var, type_generics, func_generics);
            if (i > 0) fprintf(code_stream, ", ");
            fprintf(code_stream, "%s %s", c_ty, c_val);
        });
        if (func->is_variadic) {
            if (func->args.length > 0) fprintf(code_stream, ", ");
            fprintf(code_stream, "...");
        }
        fprintf(code_stream, ") {\n");
        if (func->body->yield_last) {
            fprintf(code_stream, "    return ");
            transpile_block(program, options, code_stream, func, type_generics, func_generics, func->body, true, 1);
            fprintf(code_stream, ";\n");
        } else {
            fprintf(code_stream, "    ");
            transpile_block(program, options, code_stream, func, type_generics, func_generics, func->body, false, 1);
            fprintf(code_stream, ";\n");
            fprintf(code_stream, "    return (%s){};\n", gen_c_type_name(func->body->res, type_generics, func_generics));
        }
        fprintf(code_stream, "}\n");
        fprintf(code_stream, "\n");
    }
}

void transpile_function(Program* program, CompilerOptions* options, FILE* header_stream, FILE* code_stream, FuncDef* func) {
    if (options->verbosity >= 3) {
        if (func->impl_type == NULL) log("Transpiling function %s::%s%s", to_str_writer(s, fprint_path(s, func->module->path)), func->name->name, to_str_writer(s, fprint_generic_keys(s, func->generics)));
        else log("Transpiling method %s::%s%s", to_str_writer(s, fprint_typevalue(s, func->impl_type)), func->name->name, to_str_writer(s, fprint_generic_keys(s, func->generics)));
    }
    if (func->generics != NULL) {
        Map* dupls = map_new();
        // we can safely append to that list, even if it reallocates it should be fine as long as its the last thing we do with the item before refetching
        list_foreach(&func->generics->generic_use_keys, i, str key, {
            GenericUse* use = map_get(func->generics->generic_uses, key);
            GenericValues* type_generics = use->type_context;
            GenericValues* func_generics = use->func_context;
            if (func_generics == NULL) unreachable("func needs func generic");
            if (monomorphize(type_generics, func_generics, func->generics, use->in_func)) {
                str fn_c_name = gen_c_fn_name(func, type_generics, func_generics);
                if (!map_contains(dupls, fn_c_name)) {
                    transpile_function_generic_variant(program, options, header_stream, code_stream, func, type_generics, func_generics);
                    map_put(dupls, fn_c_name, malloc(1));
                }
            }
        });
    } else {
        transpile_function_generic_variant(program, options, header_stream, code_stream, func, NULL, NULL);
    }
}

void transpile_typedef_generic_variant(Program* program, CompilerOptions* options, FILE* header_stream, FILE* code_stream, TypeDef* ty, GenericValues* generics, bool body) {
    str modpath = to_str_writer(s, fprint_path(s, ty->module->path));
    UNUSED(code_stream);
    if (!ty->extern_ref) {
        TypeValue* tv = malloc(sizeof(TypeValue));
        tv->ctx = NULL;
        tv->def = ty;
        tv->name = NULL;
        tv->generics = generics;
        str c_name = gen_c_type_name(tv, generics, NULL);
        fprintf(header_stream, "// %s::%s%s\n", modpath, ty->name->name, gvals_to_key(tv->generics));
        fprintf(header_stream, "%s", c_name);
        if (body) {
            fprintf(header_stream, " {");
            if (map_size(ty->fields) > 0) fprintf(header_stream, "\n");
            else fprintf(header_stream, " ");
            list_foreach(&ty->flist, i, Identifier* name, {
                Field* f = map_get(ty->fields, name->name);
                str c_field_ty = gen_c_type_name(f->type, generics, NULL);
                str c_field_name = name->name;
                fprintf(header_stream, "   %s %s;\n", c_field_ty, c_field_name);
            });
            fprintf(header_stream, "}");
        }
        fprintf(header_stream, ";\n");
        fprintf(header_stream, "\n");
    }
}

void transpile_typedef(Program* program, CompilerOptions* options, FILE* header_stream, FILE* code_stream, TypeDef* ty, bool body) {
    if (ty->extern_ref || ty->transpile_state == 2) {
        return;
    }
    if (ty->transpile_state != 0) spanned_error("Recursive type", ty->name->span, "Recursion detected while transpiling struct %s", ty->name->name);
    if (options->verbosity >= 3) log("Transpiling type %s::%s", to_str_writer(s, fprint_path(s, ty->module->path)), ty->name->name);
    ty->transpile_state = 1;
    if (ty->generics != NULL) {
        // we can safely append to that list as long as its the last thing we do with the item sbefore refetching
        list_foreach(&ty->generics->generic_use_keys, i, str key, {
            GenericUse* use = map_get(ty->generics->generic_uses, key);
            GenericValues* type_generics = use->type_context;
            GenericValues* func_generics = use->func_context;
            // if (type_generics == NULL) unreachable("type needs type generics");
            // if (func_generics != NULL) unreachable("%p %p %s %s", type_generics, func_generics, ty->name->name, to_str_writer(s, fprint_span(s, &func_generics->span)));
            if (monomorphize(type_generics, func_generics, ty->generics, NULL)) { 
                map_foreach(ty->fields, str key, Field* f, {
                    UNUSED(key);
                    TypeValue* tv = f->type;
                    if (map_contains(type_generics->resolved, tv->def->name->name)) tv = map_get(type_generics->resolved, tv->def->name->name);
                    if (tv->def->transpile_state != 2 && tv->def != ty) {
                        transpile_typedef(program, options, header_stream, code_stream, tv->def, true);
                    }
                });
                transpile_typedef_generic_variant(program, options, header_stream, code_stream, ty, type_generics, body);
            }
        });
    } else {
        map_foreach(ty->fields, str key, Field* f, {
            UNUSED(key);
            TypeValue* v = f->type;
            if (v->def->transpile_state != 2 && v->def != ty) {
                transpile_typedef(program, options, header_stream, code_stream, v->def, true);
            }
        });
        transpile_typedef_generic_variant(program, options, header_stream, code_stream, ty, NULL, body);
    }
    ty->transpile_state = 2;
}

void transpile_static(Program* program, CompilerOptions* options, FILE* header_stream, FILE* code_stream, Global* s) {
    if (options->verbosity >= 3) {
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
    str c_type = gen_c_type_name(s->type, NULL, NULL);
    str c_name = gen_c_static_name(s);
    fprintf(header_stream, "extern ");
    if (is_thread_local) fprintf(header_stream, "__thread ");
    fprintf(header_stream, "%s %s;\n", c_type, c_name);
    if (!is_extern) {
        if (is_thread_local) fprintf(code_stream, "__thread ");
        if (s->computed_value != NULL) {
            if (s->computed_value->type == STRING) {
                fprintf(code_stream, "%s %s = \"%s\";\n", c_type, c_name, s->computed_value->string);
            } else {
                fprintf(code_stream, "%s %s = %s;\n", c_type, c_name, s->computed_value->string);
            }
        } else {
            fprintf(code_stream, "%s %s;\n", c_type, c_name);
        }
    }
}

void transpile_module(Program* program, CompilerOptions* options, FILE* header_stream, FILE* code_stream, Module* module, ModuleItemType type) {
    if (options->verbosity >= 2) log("Transpiling module %s [%s pass]", to_str_writer(s, fprint_path(s, module->path)), ModuleItemType__NAMES[type]);
    if (type == MIT_FUNCTION) {
        list_foreach(&module->impls, i, ImplBlock* impl, {
            map_foreach(impl->methods, str key, ModuleItem* item, {
                UNUSED(key);
                switch (item->type) {
                    case MIT_FUNCTION:
                        transpile_function(program, options, header_stream, code_stream, item->item);
                        break;
                    default:
                        unreachable("%s", ModuleItemType__NAMES[item->type]);
                }
            });
        });
    }
    map_foreach(module->items, str key, ModuleItem* item, {
        if (item->origin != NULL) continue; // we are an imported item!
        switch (item->type) {
            case MIT_MODULE:
                transpile_module(program, options, header_stream, code_stream, item->item, type);
                break;
            case MIT_FUNCTION:
                if (type == MIT_FUNCTION) transpile_function(program, options, header_stream, code_stream, item->item);
                break;
            case MIT_STRUCT:
                if (type == MIT_STRUCT) transpile_typedef(program, options, header_stream, code_stream, item->item, true);
                break;
            case MIT_STATIC:
                if (type == MIT_STATIC) transpile_static(program, options, header_stream, code_stream, item->item);
                break;
            case MIT_CONSTANT:
                break; // constants dont get transpiled, they get inlined
            case MIT_ANY: 
                unreachable();
        }
    });
}

void transpile_to_c(Program* program, CompilerOptions* options, FILE* header_stream, FILE* code_stream, str header_name) {
    program->tracegen.local_frame_name = gen_temp_c_name("local_frame");
    program->tracegen.top_frame_c_name = gen_c_var_name(program->tracegen.top_frame, NULL, NULL);
    program->tracegen.frame_type_c_name = gen_c_type_name(program->tracegen.frame_type, NULL, NULL);
    program->tracegen.function_type_c_name = gen_c_type_name(program->tracegen.function_type, NULL, NULL);

    fprintf(header_stream, "/* File generated by kommando compiler. Do not modify. */\n");
    fprintf(code_stream, "/* File generated by kommando compiler. Do not modify. */\n");

    if (!options->raw) {
        fprintf(header_stream, "#include <stdint.h>\n");
        fprintf(header_stream, "#include <stdbool.h>\n");
        fprintf(header_stream, "#include <stdalign.h>\n");
        fprintf(header_stream, "#include <assert.h>\n");
        fprintf(header_stream, "\n");
    }

    if (!options->raw) {
        StrList header_path = rsplitn(header_name, '/', 1);
        fprintf(code_stream, "#include \"%s\"\n", header_path.elements[header_path.length - 1]);
        fprintf(code_stream, "\n");
    }

    // transpile all types
    map_foreach(program->packages, str key, Module* module, {
        UNUSED(key);
        transpile_module(program, options, header_stream, code_stream, module, MIT_STRUCT);
    });

    // transpile all statics
    map_foreach(program->packages, str key, Module* module, {
        UNUSED(key);
        transpile_module(program, options, header_stream, code_stream, module, MIT_STATIC);
    });

    fprintf(code_stream, "\n");

    // transpile all functions
    map_foreach(program->packages, str key, Module* module, {
        UNUSED(key);
        transpile_module(program, options, header_stream, code_stream, module, MIT_FUNCTION);
    });

    if (!options->raw) {
        ModuleItem* main_func_item = map_get(program->main_module->items, "main");
        FuncDef* main_func = main_func_item->item;
        str main_c_name = gen_c_fn_name(main_func, NULL, NULL);
        fprintf(code_stream, "// c main entrypoint\n");
        fprintf(code_stream, "int main(int argc, char** argv) {\n");
        fprintf(code_stream, "    __global_argc = argc;\n");
        fprintf(code_stream, "    __global_argv = argv;\n");
        if (options->tracelevel > 0) {
            fprintf(code_stream, "    %s %s = { .parent=%s, .call=&TRACE_%s, .file=0, .line=0, .callflags=0b1 };\n", program->tracegen.frame_type_c_name, program->tracegen.local_frame_name, program->tracegen.top_frame_c_name, gen_default_c_fn_name(main_func, NULL, NULL));
            fprintf(code_stream, "    %s = &%s;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
        }
        fprintf(code_stream, "    %s();\n", main_c_name);
        if (options->tracelevel > 0) {
            fprintf(code_stream, "    %s = %s.parent;\n", program->tracegen.top_frame_c_name, program->tracegen.local_frame_name);
        }
        fprintf(code_stream, "    return 0;\n");
        fprintf(code_stream, "}\n");
    }
}
