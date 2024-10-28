#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "transpiler_c.h"
#include "ast.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/gc.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
#include "module.h"
#include "parser.h"
#include "resolver.h"
#include "token.h"

void fprint_resolved_typevalue(FILE* stream, TypeValue* tv, GenericValues* generics, bool long_names) {
    TypeDef* ty = tv->def;
    if (ty == NULL) panic("Unresolved type");
    if (tv->ctx != NULL) spanned_error("Indirect generic type", ty->name->span, "Type is not concrete");
    if (generics != NULL) {
        TypeValue* concrete = map_get(generics->resolved, ty->name->name);
        if (concrete != NULL) {
            return fprint_resolved_typevalue(stream, concrete, NULL, long_names);
        }
    }
    if (ty->module == NULL) {
        spanned_error("Unsubsituted generic", ty->name->span, "Genric %s. This is probably a compiler error. %s with %s", ty->name->name, gvals_to_key(tv->generics), gvals_to_key(generics));
    }
    if (long_names) {
        fprint_path(stream, ty->module->path);
        fprintf(stream, "::%s", ty->name->name);
    } else {
        fprintf(stream, "%s", ty->name->name);
    }
    if (tv->generics != NULL && tv->generics->generics.length > 0) {
        fprintf(stream, "<");
        list_foreach_i(&tv->generics->generics, lambda(void, usize i, TypeValue* v, {
            if (i > 0) fprintf(stream, ", ");
            fprint_resolved_typevalue(stream, v, generics, long_names);
        }));
        fprintf(stream, ">");
    }
} 

str gen_c_type_name(TypeValue* tv, GenericValues* generics) {
    TypeDef* ty = tv->def;
    if (ty == NULL) spanned_error("Unresolved type", tv->name->elements.elements[0]->span,  "Couldn't resolve %s", to_str_writer(s, fprint_typevalue(s, tv)));
    if (ty->extern_ref != NULL) {
        return ty->extern_ref;
    }
    if (generics != NULL) {
        TypeValue* concrete = map_get(generics->resolved, ty->name->name);
        // T == T
        // if (str_eq(to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_typevalue(s, concrete)))) {
        //    spanned_error("Weird generic", tv->name->elements.elements[0]->span, "Tried to replace %s @ %s from %s with %s @ %s from %s",
        //        to_str_writer(s, fprint_typevalue(s, tv)), to_str_writer(s, fprint_span(s, &tv->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &ty->name->span)),
        //        to_str_writer(s, fprint_typevalue(s, concrete)), to_str_writer(s, fprint_span(s, &concrete->name->elements.elements[0]->span)), to_str_writer(s, fprint_span(s, &concrete->def->name->span))
        //    );
        // }
        if (concrete != NULL && concrete->def != tv->def) {
            return gen_c_type_name(concrete, generics);
        }
    }
    if (tv->ctx != NULL) spanned_error("Indirect generic type", ty->name->span, "Type %s is not concrete", ty->name->name);
    if (ty->module == NULL) {
        spanned_error("Unsubsituted generic", ty->name->span, "Genric %s. This is probably a compiler error. %s with %s @ %s", ty->name->name, gvals_to_key(tv->generics), gvals_to_key(generics), to_str_writer(s, fprint_span(s, &generics->span)));
    }
    str key = to_str_writer(stream, {
        fprintf(stream, "%p", ty);
        if (tv->generics != NULL) {
            fprintf(stream, "<");
            list_foreach_i(&tv->generics->generics, lambda(void, usize i, TypeValue* generic_tv, {
                if (i > 0) fprintf(stream, ",");
                fprintf(stream, "%s", gen_c_type_name(generic_tv, generics));
            }));
            fprintf(stream, ">");
        }
    });
    u32 hash = str_hash(key);
    return to_str_writer(stream, fprintf(stream,"struct %s%lx", ty->name->name, hash));
}

str gen_c_fn_name(FuncDef* def, GenericValues* generics) {
    if (def->body == NULL) {
        if (def->generics != NULL) spanned_error("Generic extern function", def->name->span, "Extern functions may not be generic");
        return def->name->name;
    }
    str data = to_str_writer(stream, {
        fprintf(stream, "%p", def);
        if (generics != NULL) {
            fprintf(stream, "<");
            list_foreach_i(&generics->generics, lambda(void, usize i, TypeValue* generic_tv, {
                if (i > 0) fprintf(stream, ",");
                fprintf(stream, "%s", gen_c_type_name(generic_tv, generics));
            }));
            fprintf(stream, ">");
        }
    });
    u32 hash = str_hash(data);
    return to_str_writer(stream, fprintf(stream,"%s%lx", def->name->name, hash));
}

GenericValues* expand_generics(GenericValues* generics, GenericValues* context) {
    if (generics == NULL || context == NULL) return generics;
    GenericValues* expanded = gc_malloc(sizeof(GenericValues));
    expanded->resolved = map_new();
    expanded->span = generics->span;
    expanded->generics = list_new(TypeValueList);
    Map* rev = map_new();
    map_foreach(generics->resolved, lambda(void, str key, TypeValue* val, {
        if (str_eq(to_str_writer(s, fprint_typevalue(s, val)), "_")) spanned_error("Unresolved generic", val->name->elements.elements[0]->span, "`_`");
        map_put(rev, to_str_writer(s, fprintf(s, "%p", val)), key);
    }));
    list_foreach(&generics->generics, lambda(void, TypeValue* generic, {
        str key = to_str_writer(stream, fprint_typevalue(stream, generic));
        str real_key = map_get(rev, to_str_writer(s, fprintf(s, "%p", generic)));
        if (map_contains(context->resolved, key)) {
            TypeValue* concrete = map_get(context->resolved, key);
            list_append(&expanded->generics, concrete);
            map_put(expanded->resolved, real_key, concrete);
        } else {
            TypeValue* concrete = generic;
            GenericValues* c_e = expand_generics(concrete->generics, context);
            TypeValue* concrete_expanded = gc_malloc(sizeof(TypeValue));
            concrete_expanded->ctx = NULL;
            concrete_expanded->generics = c_e;
            concrete_expanded->def = concrete->def;
            concrete_expanded->name = concrete->name;
            list_append(&expanded->generics, concrete_expanded);
            map_put(expanded->resolved, real_key, concrete_expanded);
        }
    }));
    return expanded;
}

str gen_c_var_name(Variable* v) {
    return to_str_writer(stream, fprintf(stream,"%s%llx", v->name->name, v->id));
}

str gen_temp_c_name() {
    return to_str_writer(stream, fprintf(stream, "temp%lxt", random()));
}

void fprint_indent(FILE* file, usize indent) {
    for (usize i = 0;i < indent;i++) {
        fprintf(file, "    ");
    }
}

void transpile_block(FILE* code_stream, str modkey, FuncDef* func, GenericValues* generics, Block* block, usize indent);
void transpile_expression(FILE* code_stream, str modkey, FuncDef* func, GenericValues* generics, Expression* expr, usize indent) {
    //log("'%s' %s", ExprType__NAMES[expr->type], to_str_writer(s, fprint_span(s, &expr->span)));
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            fprintf(code_stream, "(");
            transpile_expression(code_stream, modkey, func, generics, op->lhs, indent + 1);
            fprintf(code_stream, " %s ", op->op);
            transpile_expression(code_stream, modkey, func, generics, op->rhs, indent + 1);
            fprintf(code_stream, ")");
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncDef* fd = fc->def;
            GenericValues* fc_generics = fc->generics;
            if (fc->generics != NULL) {
                fc_generics = expand_generics(fc_generics, generics);
            }
            str c_fn_name = gen_c_fn_name(fd, fc_generics);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach_i(&fc->arguments, lambda(void, usize i, Expression* arg, {
                if (i > 0) fprintf(code_stream, ", ");
                transpile_expression(code_stream, modkey, func, generics, arg, indent + 1);
            }));
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
            transpile_block(code_stream, modkey, func, generics, expr->expr, 1);
        } break;
        case EXPR_VARIABLE: {
            Variable* v = expr->expr;
            fprintf(code_stream, "%s", gen_c_var_name(v));
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;
            str c_ty = gen_c_type_name(let->type, generics);
            fprintf(code_stream, "%s %s = ", c_ty, gen_c_var_name(let->var));
            transpile_expression(code_stream, modkey, func, generics, let->value, indent + 1);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            str ret_ty = gen_c_type_name(expr->resolved->type, generics);
            str ret_var = gen_temp_c_name();
            fprintf(code_stream, "({\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "%s %s;\n", ret_ty, ret_var);
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "if (");
            transpile_expression(code_stream, modkey, func,  generics, cond->cond, indent + 1);
            fprintf(code_stream, ") %s = ", ret_var);
            transpile_block(code_stream, modkey, func,  generics, cond->then, indent + 1);
            fprintf(code_stream, "; else %s = ", ret_var);
            if (cond->otherwise != NULL) {
                transpile_block(code_stream, modkey, func,  generics, cond->otherwise, indent + 1);
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
            str ret_ty = gen_c_type_name(expr->resolved->type, generics);
            fprintf(code_stream, "({\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "while (");
            transpile_expression(code_stream, modkey, func, generics, wl->cond, indent + 1);
            fprintf(code_stream, ") ");
            transpile_block(code_stream, modkey, func,  generics, wl->body, indent + 1);
            fprintf(code_stream, ";\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "(%s) {};\n", ret_ty);
            fprint_indent(code_stream, indent);
            fprintf(code_stream, "})");
        } break;
        case EXPR_RETURN: {
            fprintf(code_stream, "({ ");
            str ret_ty = gen_c_type_name(expr->resolved->type, generics);
            if (expr->expr == NULL) {
                fprintf(code_stream, "return (%s) {};", ret_ty);
            } else {
                fprintf(code_stream, "return ");
                transpile_expression(code_stream, modkey, func, generics, expr->expr, indent + 1);
                fprintf(code_stream, ";");
            }
            fprintf(code_stream, "(%s) {}; })", ret_ty);
        } break;
        case EXPR_BREAK: {
            if (expr->expr != NULL) todo("EXPR_BREAK with expr != NULL");
            str ret_ty = gen_c_type_name(expr->resolved->type, generics);
            fprintf(code_stream, "({ break; (%s) {}; })", ret_ty);
        } break;
        case EXPR_CONTINUE: {
            str ret_ty = gen_c_type_name(expr->resolved->type, generics);
            fprintf(code_stream, "({ continue; (%s) {}; })", ret_ty);
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            transpile_expression(code_stream, modkey, func, generics, fa->object, indent);
            fprintf(code_stream, ".%s", fa->field->name);
        } break;
        case EXPR_ASSIGN: {
            Assign* assign = expr->expr;
            transpile_expression(code_stream, modkey, func, generics, assign->asignee, indent + 1);
            fprintf(code_stream, " = ");
            transpile_expression(code_stream, modkey, func, generics, assign->value, indent + 1);
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            str c_ty = gen_c_type_name(slit->type, generics);
            fprintf(code_stream, "(%s) { ", c_ty);
            usize i = 0;
            map_foreach(slit->fields, lambda(void, str _key, StructFieldLit* sfl, {
                if (i++ > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, ".%s=", sfl->name->name);
                transpile_expression(code_stream, modkey, func, generics, sfl->value, indent + 1);
            }));
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
                        str key = gc_malloc(2);
                        key[0] = c;
                        key[1] = '\0';
                        TypeValue* tv = map_get(ci->type_bindings, key);
                        if (tv == NULL) spanned_error("Invalid c intrinsic", expr->span, "intrinsic does not bind type @%s: `%s`", key, ci->c_expr);
                        if (mode == '.') {
                            fprint_resolved_typevalue(code_stream, tv, generics, false);
                        } else if (mode == ':') {
                            fprint_resolved_typevalue(code_stream, tv, generics, true);
                        } else if (mode == '!') {
                            str c_ty = gen_c_type_name(tv, generics);
                            fprintf(code_stream, "%s", c_ty);
                        } else spanned_error("Invalid c intrinsic op", expr->span, "No such mode for intrinsci operator `%c%c`", op, mode);
                    } else if (op == '$') {
                        str key = gc_malloc(2);
                        key[0] = c;
                        key[1] = '\0';
                        Variable* v = map_get(ci->var_bindings, key);
                        if (v == NULL) spanned_error("Invalid c intrinsic", expr->span, "intrinsic does not bind variale $%s: `%s`", key, ci->c_expr);
                        if (mode == ':') {
                            str name = v->name->name;
                            fprintf(code_stream, "%s", name);
                        } else if (mode == '!') {
                            str c_var = gen_c_var_name(v);
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
            fprintf(code_stream, "(*(%s*)(", gen_c_type_name(expr->resolved->type, generics));
            transpile_expression(code_stream, modkey, func, generics, inner, indent + 1);
            fprintf(code_stream, "))");
        } break;
        case EXPR_TAKEREF: {
            Expression* inner = expr->expr;
            fprintf(code_stream, "&(");
            transpile_expression(code_stream, modkey, func, generics, inner, indent + 1);
            fprintf(code_stream, ")");
        } break;
        default:
            unreachable("%s", ExprType__NAMES[expr->type]);
    }
}

void transpile_block(FILE* code_stream, str modkey, FuncDef* func, GenericValues* generics, Block* block, usize indent) {
    fprintf(code_stream, "({\n");
    list_foreach(&block->expressions, lambda(void, Expression* expr, {
        fprint_indent(code_stream, indent + 1);
        transpile_expression(code_stream, modkey, func, generics, expr, indent + 1);
        fprintf(code_stream, ";\n");
    }));
    if (!block->yield_last) {
        fprint_indent(code_stream, indent + 1);
        fprintf(code_stream, "(%s) {};\n", gen_c_type_name(block->res, generics));
    }
    fprint_indent(code_stream, indent);
    fprintf(code_stream, "})");
}

static TypeValue* find_contexted(GenericValues* instance) {
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
            log("discarding %s %p", t->def->name->name, t->generics);
            return false;
        }   
        if (!is_fully_defined(t->generics)) return false;
    }
    return true;
}

bool monomorphize(GenericValues* instance, GenericKeys* instance_host) {
    if (instance == NULL) return true;
    TypeValue* contexted = find_contexted(instance);
    if (!contexted) {
        if (!is_fully_defined(instance)) spanned_error("Not fully defined", instance->span, "This is probably a compiler error.");
        return true;
    }
    map_foreach(contexted->ctx->generic_uses, lambda(void, str key, GenericValues* context, {
        GenericValues* expanded = expand_generics(instance, context);
        str expanded_key = gvals_to_key(expanded);
        if (!map_contains(instance_host->generic_uses, expanded_key)) {
            map_put(instance_host->generic_uses, expanded_key, expanded);
            // we are iterating over this but in this case we can safely append, even if realloc
            list_append(&instance_host->generic_use_keys, expanded_key);
        }
    }));
    return false;
}


void transpile_function_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, GenericValues* generics) {
    str c_fn_name = gen_c_fn_name(func, generics);
    str c_ret_ty = gen_c_type_name(func->return_type, generics);

    fprintf(header_stream, "// %s::%s%s\n", modkey, func->name->name, gvals_to_c_key(generics));
    fprintf(header_stream, "%s %s(", c_ret_ty, c_fn_name);
    list_foreach_i(&func->args, lambda(void, usize i, Argument* arg, {
        str c_ty = gen_c_type_name(arg->type, generics);
        str c_val = gen_c_var_name(arg->var);
        if (i > 0) fprintf(header_stream, ", ");
        fprintf(header_stream, "%s %s", c_ty, c_val);
    }));
    if (func->is_variadic) {
        if (func->args.length > 0) fprintf(header_stream, ", ");
        fprintf(header_stream, "...");
    }
    fprintf(header_stream, ");\n");
    fprintf(header_stream, "\n");

    if (func->body != NULL) {
        fprintf(code_stream, "// %s::%s%s\n", modkey, func->name->name, gvals_to_c_key(generics));
        fprintf(code_stream, "%s %s(", c_ret_ty, c_fn_name);
        list_foreach_i(&func->args, lambda(void, usize i, Argument* arg, {
            str c_ty = gen_c_type_name(arg->type, generics);
            str c_val = gen_c_var_name(arg->var);
            if (i > 0) fprintf(code_stream, ", ");
            fprintf(code_stream, "%s %s", c_ty, c_val);
        }));
        if (func->is_variadic) {
            if (func->args.length > 0) fprintf(code_stream, ", ");
            fprintf(code_stream, "...");
        }
        fprintf(code_stream, ") {\n");
        fprintf(code_stream, "    return ");
        transpile_block(code_stream, modkey, func, generics, func->body, 1);
        fprintf(code_stream, ";\n");
        fprintf(code_stream, "}\n");
        fprintf(code_stream, "\n");
    }
}

void transpile_function(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func) {
    log("Transpiling function %s::%s", to_str_writer(s, fprint_path(s, func->module->path)), func->name->name);
    if (func->generics != NULL) {
        Map* dupls = map_new();
        // we can safely append to that list, even if it reallocates it should be fine
        list_foreach(&func->generics->generic_use_keys, lambda(void, str key, {
            GenericValues* generics = map_get(func->generics->generic_uses, key);
            if (monomorphize(generics, func->generics)) {
                str fn_c_name = gen_c_fn_name(func, generics);
                if (!map_contains(dupls, fn_c_name)) {
                    transpile_function_generic_variant(header_stream, code_stream, modkey, func, generics);
                    map_put(dupls, fn_c_name, malloc(1));
                }
            }
        }));
    } else {
        transpile_function_generic_variant(header_stream, code_stream, modkey, func, NULL);
    }
}

void transpile_typedef_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, TypeDef* ty, GenericValues* generics, bool body) {
    if (!ty->extern_ref) {
        TypeValue* tv = gc_malloc(sizeof(TypeValue));
        tv->ctx = NULL;
        tv->def = ty;
        tv->name = NULL;
        tv->generics = generics;
        str c_name = gen_c_type_name(tv, NULL);
        fprintf(header_stream, "// %s::%s%s\n", modkey, ty->name->name, gvals_to_key(tv->generics));
        fprintf(header_stream, "%s", c_name);
        if (body) {
            fprintf(header_stream, " {");
            if (map_size(ty->fields) > 0) fprintf(header_stream, "\n");
            else fprintf(header_stream, " ");
            map_foreach(ty->fields, lambda(void, str key, Field* f, {
                str c_field_ty = gen_c_type_name(f->type, generics);
                str c_field_name = key;
                fprintf(header_stream, "   %s %s;\n", c_field_ty, c_field_name);
            }));
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
        // we can safely append to that list
        list_foreach(&ty->generics->generic_use_keys, lambda(void, str key, {
            GenericValues* generics = map_get(ty->generics->generic_uses, key);
            if (monomorphize(generics, ty->generics)) { 
                map_foreach(ty->fields, lambda(void, str key, Field* f, {
                    TypeValue* tv = f->type;
                    if (map_contains(generics->resolved, tv->def->name->name)) tv = map_get(generics->resolved, tv->def->name->name);
                    if (tv->def->transpile_state != 2 && tv->def != ty) {
                        transpile_typedef(header_stream, code_stream, to_str_writer(s, fprint_path(s, tv->def->module->path)), tv->def, true);
                    }
                }));
                transpile_typedef_generic_variant(header_stream, code_stream, modkey, ty, generics, body);
            }
        }));
    } else {
        map_foreach(ty->fields, lambda(void, str key, Field* f, {
            TypeValue* v = f->type;
            if (v->def->transpile_state != 2 && v->def != ty) {
                transpile_typedef(header_stream, code_stream, to_str_writer(s, fprint_path(s, v->def->module->path)), v->def, true);
            }
        }));
        transpile_typedef_generic_variant(header_stream, code_stream, modkey, ty, NULL, body);
    }
    ty->transpile_state = 2;
}

void transpile_to_c(FILE* header_stream, FILE* code_stream, str header_name, Program* program) {
    fprintf(header_stream, "/* File generated by kommando compiler. Do not modify. */\n");
    fprintf(header_stream, "#include <stdint.h>\n");
    fprintf(header_stream, "#include <stdbool.h>\n");
    fprintf(header_stream, "extern void* stdin;\n");
    fprintf(header_stream, "extern void* stdout;\n");
    fprintf(header_stream, "extern void* stderr;\n");
    fprintf(header_stream, "\n");
    fprintf(header_stream, "\n");

    fprintf(code_stream, "/* File generated by kommando compiler. Do not modify. */\n");
    StrList header_path = rsplitn(header_name, '/', 1);
    fprintf(code_stream, "#include \"%s\"\n", header_path.elements[header_path.length - 1]);
    fprintf(code_stream, "\n");
    fprintf(code_stream, "static int __global__argc = 0;\n");
    fprintf(code_stream, "static char** __global__argv = (void*)0;\n");
    fprintf(code_stream, "\n");

    // transpile all types
    map_foreach(program->modules, lambda(void, str modkey, Module* module, {
        map_foreach(module->items, lambda(void, str key, ModuleItem* item, {
            switch (item->type) {
                case MIT_FUNCTION:
                    break;
                case MIT_STRUCT:
                    transpile_typedef(header_stream, code_stream, modkey, item->item, true);
                    break;
            }
        }));
    }));

    // transpile all functions
    map_foreach(program->modules, lambda(void, str modkey, Module* module, {
        map_foreach(module->items, lambda(void, str key, ModuleItem* item, {
            switch (item->type) {
                case MIT_FUNCTION:
                    transpile_function(header_stream, code_stream, modkey, item->item);
                    break;
                case MIT_STRUCT:
                    break;
            }
        }));
    }));

    ModuleItem* main_func_item = map_get(program->main_module->items, "main");
    FuncDef* main_func = main_func_item->item;
    fprintf(code_stream, "// c main entrypoint\n");
    fprintf(code_stream, "int main(int argc, char** argv) {\n");
    fprintf(code_stream, "    __global__argc = argc;\n");
    fprintf(code_stream, "    __global__argv = argv;\n");
    fprintf(code_stream, "    %s();\n", gen_c_fn_name(main_func, NULL));
    fprintf(code_stream, "    return 0;\n");
    fprintf(code_stream, "}\n");
}
