#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

str gen_c_type_name(TypeValue* tv, GenericValues* generics) {
    TypeDef* ty = tv->def;
    if (ty == NULL) panic("Unresolved type");
    if (ty->extern_ref != NULL) {
        if (ty->generics != NULL) spanned_error("Generic extern type", ty->name->span, "Extern types may not be generic");
        return ty->extern_ref;
    }
    if (generics != NULL) {
        if (generics->generic_type_ctx != NULL || generics->generic_func_ctx != NULL) spanned_error("Indirect generic type", ty->name->span, "Type is not concrete");
        TypeValue* concrete = map_get(generics->resolved, ty->name->name);
        if (concrete != NULL) {
            return gen_c_type_name(concrete, NULL);
        }
    }
    str key = to_str_writer(stream, {
        fprintf(stream, "%p", ty);
        if (tv->generics != NULL) {
            fprintf(stream, "<");
            list_foreach_i(&tv->generics->generics, lambda(void, (usize i, TypeValue* generic_tv) {
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
            list_foreach_i(&generics->generics, lambda(void, (usize i, TypeValue* generic_tv) {
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
    if ((generics->generic_func_ctx == NULL && generics->generic_type_ctx == NULL) || context == NULL) return generics;
    GenericValues* expanded = gc_malloc(sizeof(GenericValues));
    expanded->generic_func_ctx = context->generic_func_ctx;
    expanded->generic_type_ctx = context->generic_type_ctx;
    expanded->resolved = map_new();
    expanded->span = generics->span;
    expanded->generics = list_new(TypeValueList);
    Map* rev = map_new();
    map_foreach(generics->resolved, lambda(void, (str key, TypeValue* val) {
        map_put(rev, to_str_writer(s, fprintf(s, "%p", val)), key);
    }));
    list_foreach(&generics->generics, lambda(void, (TypeValue* generic) {
        str key = to_str_writer(stream, fprint_typevalue(stream, generic));
        str real_key = map_get(rev, to_str_writer(s, fprintf(s, "%p", generic)));
        if (map_contains(context->resolved, key)) {
            TypeValue* concrete = map_get(context->resolved, key);
            list_append(&expanded->generics, concrete);
            map_put(expanded->resolved, real_key, concrete);
        } else {
            list_append(&expanded->generics, generic);
            map_put(expanded->resolved, real_key, generic);
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
            list_foreach_i(&fc->arguments, lambda(void, (usize i, Expression* arg) {
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
            todo("EXPR_WHILE_LOOP");
        } break;
        case EXPR_RETURN: {
            todo("EXPR_RETURN");
        } break;
        case EXPR_BREAK: {
            todo("EXPR_BREAK");
        } break;
        case EXPR_CONTINUE: {
            todo("EXPR_CONTINUE");
        } break;
        case EXPR_FIELD_ACCESS: {
            FieldAccess* fa = expr->expr;
            transpile_expression(code_stream, modkey, func, generics, fa->object, indent);
            fprintf(code_stream, ".%s", fa->field->name);
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            str c_ty = gen_c_type_name(slit->type, generics);
            fprintf(code_stream, "(%s) { ", c_ty);
            usize i = 0;
            map_foreach(slit->fields, lambda(void, (str _key, StructFieldLit* sfl) {
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
            while (i < len) {
                char c = ci->c_expr[i++];
                if (op != '\0') {
                    if (op == '@') {
                        str key = gc_malloc(2);
                        key[0] = c;
                        key[1] = '\0';
                        TypeValue* tv = map_get(ci->type_bindings, key);
                        if (tv == NULL) spanned_error("Invalid c intrinsic", expr->span, "intrinsic does not bind type @%s: `%s`", key, ci->c_expr);
                        str c_ty = gen_c_type_name(tv, generics);
                        fprintf(code_stream, "%s", c_ty);
                    } else if (op == '$') {
                        str key = gc_malloc(2);
                        key[0] = c;
                        key[1] = '\0';
                        Variable* v = map_get(ci->var_bindings, key);
                        if (v == NULL) spanned_error("Invalid c intrinsic", expr->span, "intrinsic does not bind variale $%s: `%s`", key, ci->c_expr);
                        str c_var = gen_c_var_name(v);
                        fprintf(code_stream, "%s", c_var);
                    } else {
                        unreachable();
                    }
                    op = '\0';
                } else if (c == '@' || c == '$') {
                    op = c;
                } else {
                    fputc(c, code_stream);
                }
            }
            if (op != '\0') spanned_error("Invalid c intrinsic", expr->span, "intrinsic ended on operator: `%s`", ci->c_expr);
        } break;
    }
}

void transpile_block(FILE* code_stream, str modkey, FuncDef* func, GenericValues* generics, Block* block, usize indent) {
    fprintf(code_stream, "({\n");
    list_foreach(&block->expressions, lambda(void, (Expression* expr) {
        fprint_indent(code_stream, indent + 1);
        transpile_expression(code_stream, modkey, func, generics, expr, indent + 1);
        fprintf(code_stream, ";\n", gen_c_type_name(block->res, generics));
    }));
    if (!block->yield_last) {
        fprint_indent(code_stream, indent + 1);
        fprintf(code_stream, "(%s) {};\n", gen_c_type_name(block->res, generics));
    }
    fprint_indent(code_stream, indent);
    fprintf(code_stream, "})");
}

void transpile_function_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, GenericValues* generics) {
    str c_fn_name = gen_c_fn_name(func, generics);
    str c_ret_ty = gen_c_type_name(func->return_type, generics);

    fprintf(header_stream, "// %s::%s%s\n", modkey, func->name->name, gvals_to_c_key(generics));
    fprintf(header_stream, "%s %s(", c_ret_ty, c_fn_name);
    list_foreach_i(&func->args, lambda(void, (usize i, Argument* arg) {
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
        list_foreach_i(&func->args, lambda(void, (usize i, Argument* arg) {
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

bool monomorphize(GenericValues* instance, GenericKeys* instance_host) {
    if (instance->generic_func_ctx == NULL && instance->generic_type_ctx == NULL) return true;
    if (instance->generic_func_ctx != NULL && instance->generic_type_ctx != NULL) todo();
    if (instance->generic_func_ctx != NULL) {
        map_foreach(instance->generic_func_ctx->generic_uses, lambda(void, (str _key, GenericValues* context) {
            GenericValues* expanded = expand_generics(instance, context);
            str expanded_key = gvals_to_key(expanded);
            if (!map_contains(instance_host->generic_uses, expanded_key)) {
                map_put(instance_host->generic_uses, expanded_key, expanded);
                // we are iterating over this but in this case we can safely append, even if realloc
                list_append(&instance_host->generic_use_keys, expanded_key);
            }
        }));
    }
    if (instance->generic_type_ctx != NULL) {
        map_foreach(instance->generic_type_ctx->generic_uses, lambda(void, (str _key, GenericValues* context) {
            GenericValues* expanded = expand_generics(instance, context);
            str expanded_key = gvals_to_key(expanded);
            if (!map_contains(instance_host->generic_uses, expanded_key)) {
                map_put(instance_host->generic_uses, expanded_key, expanded);
                // we are iterating over this but in this case we can safely append, even if realloc
                list_append(&instance_host->generic_use_keys, expanded_key);
            }
        }));
    }
    return false;
}

void transpile_function(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func) {
    if (func->generics != NULL) {
        // we can safely append to that list, even if it reallocates it should be fine
        list_foreach(&func->generics->generic_use_keys, lambda(void, (str key) {
            GenericValues* generics = map_get(func->generics->generic_uses, key);
            if (monomorphize(generics, func->generics)) {
                transpile_function_generic_variant(header_stream, code_stream, modkey, func, generics);
            }
        }));
    } else {
        transpile_function_generic_variant(header_stream, code_stream, modkey, func, NULL);
    }
}

void transpile_typedef_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, TypeDef* ty, GenericValues* generics, bool body) {
    if (!ty->extern_ref) {
        TypeValue* tv = gc_malloc(sizeof(TypeValue));
        tv->def = ty;
        tv->name = NULL;
        tv->generics = generics;
        str c_name = gen_c_type_name(tv, NULL);
        fprintf(header_stream, "// %s::%s%s\n", modkey, ty->name->name, gvals_to_key(tv->generics));
        fprintf(header_stream, "%s", c_name);
        if (body) {
            fprintf(header_stream, " {\n");
            map_foreach(ty->fields, lambda(void, (str key, Field* f) {
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
    if (ty->extern_ref) {
        return;
    }
    if (ty->generics != NULL) {
        // we can safely append to that list
        list_foreach(&ty->generics->generic_use_keys, lambda(void, (str key) {
            GenericValues* generics = map_get(ty->generics->generic_uses, key);
            if (monomorphize(generics, ty->generics)) {
                transpile_typedef_generic_variant(header_stream, code_stream, modkey, ty, generics, body);
            }
        }));
    } else {
        transpile_typedef_generic_variant(header_stream, code_stream, modkey, ty, NULL, body);
    }
}


void transpile_module(FILE* header_stream, FILE* code_stream, str modkey, Module* module, usize stage) {
    if (stage == 1) map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            break;
        case MIT_STRUCT:
            transpile_typedef(header_stream, code_stream, modkey, item->item, false);
            break;
        }
    }));
    if (stage == 2) map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            break;
        case MIT_STRUCT:
            transpile_typedef(header_stream, code_stream, modkey, item->item, true);
            break;
        }
    }));
    if (stage == 3) map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            transpile_function(header_stream, code_stream, modkey, item->item);
            break;
        case MIT_STRUCT:
            break;
        }
    }));
}

void transpile_to_c(FILE* header_stream, FILE* code_stream, str header_name, Program* program) {
    fprintf(header_stream, "/* File generated by kommando compiler. Do not modify. */\n");
    fprintf(header_stream, "#include <stdint.h>\n");
    fprintf(header_stream, "#include <stdbool.h>\n");
    fprintf(header_stream, "\n");
    fprintf(header_stream, "\n");

    fprintf(code_stream, "/* File generated by kommando compiler. Do not modify. */\n");
    fprintf(code_stream, "#include \"%s\"\n", header_name);
    fprintf(code_stream, "\n");
    fprintf(code_stream, "static int __global__argc = 0;\n");
    fprintf(code_stream, "static char** __global__argv = (void*)0;\n");
    fprintf(code_stream, "\n");

    map_foreach(program->modules, lambda(void, (str key, Module* mod) {
        transpile_module(header_stream, code_stream, key, mod, 1);
    }));
    map_foreach(program->modules, lambda(void, (str key, Module* mod) {
        transpile_module(header_stream, code_stream, key, mod, 2);
    }));
    map_foreach(program->modules, lambda(void, (str key, Module* mod) {
        transpile_module(header_stream, code_stream, key, mod, 3);
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
