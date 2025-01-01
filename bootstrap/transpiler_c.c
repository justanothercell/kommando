#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lib.h"
#include "lib/exit.h"
#include "lib/map.h"
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

str gen_c_type_name(TypeValue* tv, GenericValues* type_generics, GenericValues* func_generics) {
    TypeDef* ty = tv->def;
    if (ty == NULL) spanned_error("Unresolved type", tv->name->elements.elements[0]->span,  "Couldn't resolve %s", to_str_writer(s, fprint_typevalue(s, tv)));
    if (ty->extern_ref != NULL) {
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
            return gen_c_type_name(concrete, func_generics, type_generics);
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
                fprintf(stream, "%s", gen_c_type_name(generic_tv, func_generics, type_generics));
            });
            fprintf(stream, ">");
        }
    });
    u32 hash = str_hash(key);
    return to_str_writer(stream, fprintf(stream,"struct %s%lx", ty->name->name, hash));
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
            name = def->name->name;
            state = 2;
        }
    });
    if (name != NULL) return name;
    str data = to_str_writer(stream, {
        fprintf(stream, "%p%s", def, tfvals_to_key(type_generics, func_generics));
    });
    u32 hash = str_hash(data);
    return to_str_writer(stream, fprintf(stream,"%s%lx", def->name->name, hash));
}

GenericValues* expand_generics(GenericValues* generics, GenericValues* type_context, GenericValues* func_context) {
    if (generics == NULL || (type_context == NULL && func_context == NULL)) return generics;
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
            list_append(&expanded->generics, concrete);
            map_put(expanded->resolved, real_key, concrete);
        } else {
            TypeValue* concrete = generic;
            GenericValues* c_e = expand_generics(concrete->generics, type_context, func_context);
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

str gen_c_static_name(Static* s) {
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
    if (v->static_ref != NULL) {
        return gen_c_static_name(v->static_ref);
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

str gen_temp_c_name() {
    return to_str_writer(stream, fprintf(stream, "temp%lxt", random()));
}

void fprint_indent(FILE* file, usize indent) {
    for (usize i = 0;i < indent;i++) {
        fprintf(file, "    ");
    }
}

void transpile_block(FILE* code_stream, str modkey, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Block* block, usize indent);
void transpile_expression(FILE* code_stream, str modkey, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Expression* expr, usize indent) {
    //log("'%s' %s", ExprType__NAMES[expr->type], to_str_writer(s, fprint_span(s, &expr->span)));
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            fprintf(code_stream, "(");
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, op->lhs, indent + 1);
            fprintf(code_stream, " %s ", op->op);
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, op->rhs, indent + 1);
            fprintf(code_stream, ")");
        } break;
        case EXPR_BIN_OP_ASSIGN: {
            BinOp* op = expr->expr;
            str temp = gen_temp_c_name();
            str ty = gen_c_type_name(op->lhs->resolved->type, type_generics, func_generics);
            fprintf(code_stream, "%s* %s = &(", ty, temp);
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, op->lhs, indent + 1);
            fprintf(code_stream, "); *%s = *%s %s ", temp, temp, op->op);
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, op->rhs, indent + 1);
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncDef* fd = fc->def;
            GenericValues* fc_generics = fc->generics;
            if (fc->generics != NULL) {
                fc_generics = expand_generics(fc_generics, type_generics, func_generics);
            }
            str c_fn_name = gen_c_fn_name(fd, NULL, fc_generics);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach(&fc->arguments, i, Expression* arg, {
                if (i > 0) fprintf(code_stream, ", ");
                transpile_expression(code_stream, modkey, func, type_generics, func_generics, arg, indent + 1);
            });
            fprintf(code_stream, ")");
        } break;
        case EXPR_METHOD_CALL: {
            MethodCall* call = expr->expr;
            FuncDef* fd = call->def->func;
            GenericValues* call_generics = expand_generics(call->generics, type_generics, func_generics);
            GenericValues* type_call_generics = expand_generics(call->impl_vals, type_generics, func_generics);
            str c_fn_name = gen_c_fn_name(fd, type_call_generics, call_generics);
            fprintf(code_stream, "%s(", c_fn_name);
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, call->object, indent + 1);
            if (call->arguments.length > 0) fprintf(code_stream, ", ");
            list_foreach(&call->arguments, i, Expression* arg, {
                if (i > 0) fprintf(code_stream, ", ");
                transpile_expression(code_stream, modkey, func, type_generics, func_generics, arg, indent + 1);
            });
            fprintf(code_stream, ")");
        } break;
        case EXPR_STATIC_METHOD_CALL: {
            StaticMethodCall* call = expr->expr;
            FuncDef* fd = call->def->func;
            GenericValues* call_generics = expand_generics(call->generics, type_generics, func_generics);
            GenericValues* type_call_generics = expand_generics(call->impl_vals, type_generics, func_generics);
            str c_fn_name = gen_c_fn_name(fd, type_call_generics, call_generics);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach(&call->arguments, i, Expression* arg, {
                if (i > 0) fprintf(code_stream, ", ");
                transpile_expression(code_stream, modkey, func, type_generics, func_generics, arg, indent + 1);
            });
            fprintf(code_stream, ")");
        } break;
        case EXPR_DYN_RAW_CALL: {
            DynRawCall* fc = expr->expr;
            fprintf(code_stream, "(");
            str c_ret = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            fprintf(code_stream, "(%s(*)())", c_ret);
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, fc->callee, indent + 1);
            fprintf(code_stream, ")");
            fprintf(code_stream, "(");
            list_foreach(&fc->args, i, Expression* arg, {
                if (i > 0) fprintf(code_stream, ", ");
                transpile_expression(code_stream, modkey, func, type_generics, func_generics, arg, indent + 1);
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
            transpile_block(code_stream, modkey, func, type_generics, func_generics, expr->expr, 1);
        } break;
        case EXPR_VARIABLE: {
            Variable* v = expr->expr;
            fprintf(code_stream, "%s", gen_c_var_name(v, type_generics, func_generics));
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;
            str c_ty = gen_c_type_name(let->type, type_generics, func_generics);
            str c_name = gen_c_var_name(let->var, type_generics, func_generics);
            fprintf(code_stream, "%s %s = ", c_ty, c_name);
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, let->value, indent + 1);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            str ret_var = gen_temp_c_name();
            fprintf(code_stream, "({\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "%s %s;\n", ret_ty, ret_var);
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "if (");
            transpile_expression(code_stream, modkey, func,  type_generics, func_generics, cond->cond, indent + 1);
            fprintf(code_stream, ") %s = ", ret_var);
            transpile_block(code_stream, modkey, func,  type_generics, func_generics, cond->then, indent + 1);
            fprintf(code_stream, "; else %s = ", ret_var);
            if (cond->otherwise != NULL) {
                transpile_block(code_stream, modkey, func,  type_generics, func_generics, cond->otherwise, indent + 1);
            } else {
                fprintf(code_stream, "(%s) {}", ret_ty);
            }
            fprintf(code_stream, ";\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "%s;\n", ret_var);
            fprint_indent(code_stream, indent);
            fprintf(code_stream, "})");
        } break;
        case EXPR_WHILE_LOOP: {
            WhileLoop* wl = expr->expr;
            str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            fprintf(code_stream, "({\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "while (");
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, wl->cond, indent + 1);
            fprintf(code_stream, ") ");
            transpile_block(code_stream, modkey, func,  type_generics, func_generics, wl->body, indent + 1);
            fprintf(code_stream, ";\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "(%s) {};\n", ret_ty);
            fprint_indent(code_stream, indent);
            fprintf(code_stream, "})");
        } break;
        case EXPR_RETURN: {
            fprintf(code_stream, "({ ");
            str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            if (expr->expr == NULL) {
                fprintf(code_stream, "return (%s) {};", ret_ty);
            } else {
                fprintf(code_stream, "return ");
                transpile_expression(code_stream, modkey, func, type_generics, func_generics, expr->expr, indent + 1);
                fprintf(code_stream, ";");
            }
            fprintf(code_stream, "(%s) {}; })", ret_ty);
        } break;
        case EXPR_BREAK: {
            if (expr->expr != NULL) todo("EXPR_BREAK with expr != NULL");
            str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            fprintf(code_stream, "({ break; (%s) {}; })", ret_ty);
        } break;
        case EXPR_CONTINUE: {
            str ret_ty = gen_c_type_name(expr->resolved->type, type_generics, func_generics);
            fprintf(code_stream, "({ continue; (%s) {}; })", ret_ty);
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, fa->object, indent);
            fprintf(code_stream, ".%s", fa->field->name);
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, assign->asignee, indent + 1);
            fprintf(code_stream, " = ");
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, assign->value, indent + 1);
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
                transpile_expression(code_stream, modkey, func, type_generics, func_generics, sfl->value, indent + 1);
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
                        } else spanned_error("Invalid c intrinsic op", expr->span, "No such mode for intrinsci operator `%c%c`", op, mode);
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
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, inner, indent + 1);
            fprintf(code_stream, "))");
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            fprintf(code_stream, "&(");
            transpile_expression(code_stream, modkey, func, type_generics, func_generics, inner, indent + 1);
            fprintf(code_stream, ")");
        } break;
        default:
            unreachable("%s", ExprType__NAMES[expr->type]);
    }
}

void transpile_block(FILE* code_stream, str modkey, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics, Block* block, usize indent) {
    fprintf(code_stream, "({\n");
    list_foreach(&block->expressions, i, Expression* expr, {
        fprint_indent(code_stream, indent + 1);
        transpile_expression(code_stream, modkey, func, type_generics, func_generics, expr, indent + 1);
        fprintf(code_stream, ";\n");
    });
    if (!block->yield_last) {
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

bool monomorphize(GenericValues* type_instance, GenericValues* func_instance, GenericKeys* instance_host) {
    if (type_instance != NULL) {
        TypeValue* type_contexted = find_contexted(type_instance);
        if (type_contexted == NULL) {
            if (!is_fully_defined(type_instance)) spanned_error("Not fully defined", type_instance->span, "This is probably a compiler error. For now just supply more generic hints. Defined @ %s", to_str_writer(s, fprint_span(s, &instance_host->span)));
            goto type_done;
        }
        map_foreach(type_contexted->ctx->generic_uses, str key, GenericUse* use, {
            log(" -> %s ", key);
            UNUSED(key);
            GenericValues* expanded = expand_generics(type_instance, use->type_context, use->func_context);
            str expanded_key = tfvals_to_key(expanded, func_instance);
            if (!map_contains(instance_host->generic_uses, expanded_key)) {
                GenericUse* use = malloc(sizeof(GenericUse));
                use->type_context = expanded;
                use->func_context = func_instance;
                map_put(instance_host->generic_uses, expanded_key, use);
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
        log(" > found (func) contexted %s with %llu uses", to_str_writer(s, fprint_typevalue(s, func_contexted)), map_size(func_contexted->ctx->generic_uses));
        map_foreach(func_contexted->ctx->generic_uses, str key, GenericUse* use, {
            UNUSED(key);
            GenericValues* expanded = expand_generics(func_instance, use->type_context, use->func_context);
            str expanded_key = tfvals_to_key(type_instance, expanded);
            if (!map_contains(instance_host->generic_uses, expanded_key)) {
                GenericUse* use = malloc(sizeof(GenericUse));
                use->type_context = type_instance;
                use->func_context = expanded;
                map_put(instance_host->generic_uses, expanded_key, use);
                // we are iterating over this but in this case we can safely append, even if realloc
                list_append(&instance_host->generic_use_keys, expanded_key);
            }
        });
        return false;
    }
    func_done: {}
    return true;
}


void transpile_function_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, GenericValues* type_generics, GenericValues* func_generics) {
    str c_fn_name = gen_c_fn_name(func, type_generics, func_generics);
    str c_ret_ty = gen_c_type_name(func->return_type, type_generics, func_generics);
    if (func->impl_type == NULL) fprintf(header_stream, "// %s::%s%s%s\n", modkey, func->name->name, gvals_to_c_key(type_generics), gvals_to_c_key(func_generics));
    else fprintf(header_stream, "// %s%s::%s%s\n", to_str_writer(s, fprint_td_path(s, func->impl_type->def)), gvals_to_c_key(type_generics), func->name->name, gvals_to_c_key(func_generics));
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
        if (func->impl_type == NULL) fprintf(code_stream, "// %s::%s%s%s\n", modkey, func->name->name, gvals_to_c_key(type_generics), gvals_to_c_key(func_generics));
        else fprintf(code_stream, "// %s%s::%s%s\n", to_str_writer(s, fprint_td_path(s, func->impl_type->def)), gvals_to_c_key(type_generics), func->name->name, gvals_to_c_key(func_generics));
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
        fprintf(code_stream, "    return ");
        transpile_block(code_stream, modkey, func, type_generics, func_generics, func->body, 1);
        fprintf(code_stream, ";\n");
        fprintf(code_stream, "}\n");
        fprintf(code_stream, "\n");
    }
}

void transpile_function(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func) {
    if (func->impl_type == NULL) log("Transpiling function %s::%s", to_str_writer(s, fprint_path(s, func->module->path)), func->name->name);
    else log("Transpiling method %s::%s", to_str_writer(s, fprint_typevalue(s, func->impl_type)), func->name->name);
    if (func->generics != NULL) {
        Map* dupls = map_new();
        // we can safely append to that list, even if it reallocates it should be fine as long as its the last thing we do with the item sbefore refetching
        list_foreach(&func->generics->generic_use_keys, i, str key, {
            GenericUse* use = map_get(func->generics->generic_uses, key);
            GenericValues* type_generics = use->type_context;
            GenericValues* func_generics = use->func_context;
            log("variant %s %lld", key, func->generics->generic_use_keys.length);
            if (monomorphize(type_generics, func_generics, func->generics)) {
                str fn_c_name = gen_c_fn_name(func, type_generics, func_generics);
                if (!map_contains(dupls, fn_c_name)) {
                    transpile_function_generic_variant(header_stream, code_stream, modkey, func, type_generics, func_generics);
                    map_put(dupls, fn_c_name, malloc(1));
                }
            }
        });
    } else {
        transpile_function_generic_variant(header_stream, code_stream, modkey, func, NULL, NULL);
    }
}

void transpile_typedef_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, TypeDef* ty, GenericValues* generics, bool body) {
    UNUSED(code_stream);
    if (!ty->extern_ref) {
        TypeValue* tv = malloc(sizeof(TypeValue));
        tv->ctx = NULL;
        tv->def = ty;
        tv->name = NULL;
        tv->generics = generics;
        str c_name = gen_c_type_name(tv, generics, NULL);
        fprintf(header_stream, "// %s::%s%s\n", modkey, ty->name->name, gvals_to_key(tv->generics));
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

void transpile_typedef(FILE* header_stream, FILE* code_stream, str modkey, TypeDef* ty, bool body) {
    if (ty->extern_ref || ty->transpile_state == 2) {
        return;
    }
    if (ty->transpile_state != 0) spanned_error("Recursive type", ty->name->span, "Recursion detected while transpiling struct %s", ty->name->name);
    log("Transpiling type %s::%s", to_str_writer(s, fprint_path(s, ty->module->path)), ty->name->name);
    ty->transpile_state = 1;
    if (ty->generics != NULL) {
        // we can safely append to that list as long as its the last thing we do with the item sbefore refetching
        list_foreach(&ty->generics->generic_use_keys, i, str key, {
            GenericUse* use = map_get(ty->generics->generic_uses, key);
            GenericValues* type_generics = use->type_context;
            GenericValues* func_generics = use->func_context;
            if (func_generics != NULL) unreachable("%p %p %s %s", type_generics, func_generics, ty->name->name, to_str_writer(s, fprint_span(s, &func_generics->span)));
            if (monomorphize(type_generics, func_generics, ty->generics)) { 
                map_foreach(ty->fields, str key, Field* f, {
                    UNUSED(key);
                    TypeValue* tv = f->type;
                    if (map_contains(type_generics->resolved, tv->def->name->name)) tv = map_get(type_generics->resolved, tv->def->name->name);
                    if (tv->def->transpile_state != 2 && tv->def != ty) {
                        transpile_typedef(header_stream, code_stream, to_str_writer(s, fprint_path(s, tv->def->module->path)), tv->def, true);
                    }
                });
                transpile_typedef_generic_variant(header_stream, code_stream, modkey, ty, type_generics, body);
            }
        });
    } else {
        map_foreach(ty->fields, str key, Field* f, {
            UNUSED(key);
            TypeValue* v = f->type;
            if (v->def->transpile_state != 2 && v->def != ty) {
                transpile_typedef(header_stream, code_stream, to_str_writer(s, fprint_path(s, v->def->module->path)), v->def, true);
            }
        });
        transpile_typedef_generic_variant(header_stream, code_stream, modkey, ty, NULL, body);
    }
    ty->transpile_state = 2;
}

void transpile_static(FILE* header_stream, FILE* code_stream, str modkey, Static* s) {
    UNUSED(modkey);
    log("transpiling static %s::%s", to_str_writer(stream, fprint_path(stream, s->module->path)), s->name->name);
    bool is_extern = false;
    list_foreach(&s->annotations, i, Annotation a, {
        str p = to_str_writer(s, fprint_path(s, a.path));
        if (str_eq(p, "extern")) {            
            if (is_extern) spanned_error("Duplicate annotation flag", a.path->elements.elements[0]->span, "Flag `#[extern]` already set");
            if (a.type != AT_FLAG) spanned_error("Invalid annotation type", a.path->elements.elements[0]->span, "Expected flag `#[extern]`, found %s", AnnotationType__NAMES[a.type]);
            is_extern = true;
        }
    });
    str c_type = gen_c_type_name(s->type, NULL, NULL);
    str c_name = gen_c_static_name(s);
    fprintf(header_stream, "extern %s %s;\n", c_type, c_name);
    if (!is_extern) {
        fprintf(code_stream, "%s %s;\n", c_type, c_name);
    }
}

void transpile_module(FILE* header_stream, FILE* code_stream, str modkey, Module* module, ModuleItemType type) {
    log("Transpiling module %s [%s pass]", to_str_writer(s, fprint_path(s, module->path)), ModuleItemType__NAMES[type]);
    if (type == MIT_FUNCTION) {
        list_foreach(&module->impls, i, ImplBlock* impl, {
            map_foreach(impl->methods, str key, ModuleItem* item, {
                UNUSED(key);
                switch (item->type) {
                    case MIT_FUNCTION:
                        transpile_function(header_stream, code_stream, modkey, item->item);
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
                transpile_module(header_stream, code_stream, key, item->item, type);
                break;
            case MIT_FUNCTION:
                if (type == MIT_FUNCTION) transpile_function(header_stream, code_stream, modkey, item->item);
                break;
            case MIT_STRUCT:
                if (type == MIT_STRUCT) transpile_typedef(header_stream, code_stream, modkey, item->item, true);
                break;
            case MIT_STATIC: 
                if (type == MIT_STATIC) transpile_static(header_stream, code_stream, modkey, item->item);
                break;
            case MIT_ANY: 
                unreachable();
        }
    });
}

void transpile_to_c(CompilerOptions options, FILE* header_stream, FILE* code_stream, str header_name, Program* program) {
    fprintf(header_stream, "/* File generated by kommando compiler. Do not modify. */\n");
    fprintf(code_stream, "/* File generated by kommando compiler. Do not modify. */\n");

    if (!options.raw) {
        fprintf(header_stream, "#include <stdint.h>\n");
        fprintf(header_stream, "#include <stdbool.h>\n");
        fprintf(header_stream, "#include <stdalign.h>\n");
        fprintf(header_stream, "#include <assert.h>\n");
        fprintf(header_stream, "\n");
    }

    if (!options.raw) {
        StrList header_path = rsplitn(header_name, '/', 1);
        fprintf(code_stream, "#include \"%s\"\n", header_path.elements[header_path.length - 1]);
        fprintf(code_stream, "\n");
    }

    // transpile all types
    map_foreach(program->packages, str modkey, Module* module, {
        transpile_module(header_stream, code_stream, modkey, module, MIT_STRUCT);
    });

    // transpile all statics
    map_foreach(program->packages, str modkey, Module* module, {
        transpile_module(header_stream, code_stream, modkey, module, MIT_STATIC);
    });

    fprintf(code_stream, "\n");

    // transpile all functions
    map_foreach(program->packages, str modkey, Module* module, {
        transpile_module(header_stream, code_stream, modkey, module, MIT_FUNCTION);
    });

    if (!options.raw) {
        ModuleItem* main_func_item = map_get(program->main_module->items, "main");
        FuncDef* main_func = main_func_item->item;
        fprintf(code_stream, "// c main entrypoint\n");
        fprintf(code_stream, "int main(int argc, char** argv) {\n");
        fprintf(code_stream, "    __global_argc = argc;\n");
        fprintf(code_stream, "    __global_argv = argv;\n");
        fprintf(code_stream, "    %s();\n", gen_c_fn_name(main_func, NULL, NULL));
        fprintf(code_stream, "    return 0;\n");
        fprintf(code_stream, "}\n");
    }
}
