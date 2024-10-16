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

str gen_c_fn_name(FuncDef* def, str fukey) {
    if (def->body == NULL) {
        // if (map_size(def->) > 0) spanned_error("Generic extern function", def->name->span, "Extern functions may not be generic");
        return def->name->name;
    }
    u32 hash = str_hash(fukey);
    return to_str_writer(stream, fprintf(stream,"%s%lx", def->name->name, hash));
}

str gen_c_var_name(Variable* v) {
    return to_str_writer(stream, fprintf(stream,"%s%llu", v->name->name, v->id));
}

str gen_temp_c_name() {
    return to_str_writer(stream, fprintf(stream, "temp%lu", random()));
}

str gen_c_type_name(TypeValue* tv, FuncUsage* fu) {
    if (tv->def->extern_ref != NULL) {
        if (tv->generics != NULL && tv->generics->generics.length > 0) spanned_error("Generic extern type", tv->def->name->span, "Extern types may not be generic");
        return tv->def->extern_ref;
    }
    if (tv->generics != NULL && tv->generics->generics.length > 0) todo("Generic types");
    if (fu != NULL && fu->generic_use != NULL)  spanned_error("indirect generic type", tv->def->name->span, "Type is not concrete");
    if (fu != NULL && fu->generics != NULL && !tv->name->absolute && tv->name->elements.length == 1 && map_contains(fu->generics, tv->name->elements.elements[0]->name)) {
        TypeValue* concrete = map_get(fu->generics, tv->name->elements.elements[0]->name);
        return gen_c_type_name(concrete, NULL);
    }
    return to_str_writer(stream, fprintf(stream,"struct %s", tv->def->name->name));
}

void fprint_indent(FILE* file, usize indent) {
    for (usize i = 0;i < indent;i++) {
        fprintf(file, "    ");
    }
}

void transpile_block(FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu, Block* block, usize indent);
void transpile_expression(FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu, Expression* expr, usize indent) {
    switch (expr->type) {
        case EXPR_BIN_OP: {
            BinOp* op = expr->expr;
            fprintf(code_stream, "(");
            transpile_expression(code_stream, modkey, func, fukey, fu, op->lhs, indent + 1);
            fprintf(code_stream, " %s ", op->op);
            transpile_expression(code_stream, modkey, func, fukey, fu, op->rhs, indent + 1);
            fprintf(code_stream, ")");
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            FuncUsage* fc_fu = fc->fu;
            if (fc_fu->generic_use != NULL) {
                FuncUsage* expanded = gc_malloc(sizeof(FuncUsage));
                expanded->generic_use = NULL;
                expanded->generics = map_new();
                map_foreach(fc_fu->generics, lambda(void, (str key, TypeValue* tv) {
                    str valkey = to_str_writer(s, fprint_typevalue(s, tv));
                    if (map_contains(fu->generics, valkey)) {
                        map_put(expanded->generics, key, map_get(fu->generics, valkey));
                    } else {
                        map_put(expanded->generics, key, tv);
                    }
                }));
                fc_fu = expanded;
            }
            str key = funcusage_to_key(fc_fu);
            str c_fn_name = gen_c_fn_name(fc->def, key);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach_i(&fc->arguments, lambda(void, (usize i, Expression* arg) {
                if (i > 0) fprintf(code_stream, ", ");
                transpile_expression(code_stream, modkey, func, fukey, fu, arg, indent + 1);
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
            transpile_block(code_stream, modkey, func, fukey, fu, expr->expr, 1);
        } break;
        case EXPR_VARIABLE: {
            Variable* v = expr->expr;
            fprintf(code_stream, "%s", gen_c_var_name(v));
        } break;
        case EXPR_LET: {
            LetExpr* let = expr->expr;
            str c_ty = gen_c_type_name(let->type, fu);
            fprintf(code_stream, "%s %s = ", c_ty, gen_c_var_name(let->var));
            transpile_expression(code_stream, modkey, func, fukey, fu, let->value, indent + 1);
        } break;
        case EXPR_CONDITIONAL: {
            Conditional* cond = expr->expr;
            str ret_ty = gen_c_type_name(expr->resolved->type, fu);
            str ret_var = gen_temp_c_name();
            fprintf(code_stream, "({\n");
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "%s %s;\n", ret_ty, ret_var);
            fprint_indent(code_stream, indent + 1);
            fprintf(code_stream, "if (");
            transpile_expression(code_stream, modkey, func, fukey, fu, cond->cond, indent + 1);
            fprintf(code_stream, ") %s = ", ret_var);
            transpile_block(code_stream, modkey, func, fukey, fu, cond->then, indent + 1);
            fprintf(code_stream, "; else %s = ", ret_var);
            if (cond->otherwise != NULL) {
                transpile_block(code_stream, modkey, func, fukey, fu, cond->otherwise, indent + 1);
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
            transpile_expression(code_stream, modkey, func,fukey, fu, fa->object, indent);
            fprintf(code_stream, ".%s", fa->field->name);
        } break;
        case EXPR_STRUCT_LITERAL: {
            StructLiteral* slit = expr->expr;
            str c_ty = gen_c_type_name(slit->type, fu);
            fprintf(code_stream, "(%s) { ", c_ty);
            usize i = 0;
            map_foreach(slit->fields, lambda(void, (str _key, StructFieldLit* sfl) {
                if (i++ > 0) fprintf(code_stream, ", ");
                fprintf(code_stream, ".%s=", sfl->name->name);
                transpile_expression(code_stream, modkey, func,fukey, fu, sfl->value, indent + 1);
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
                        str c_ty = gen_c_type_name(tv, fu);
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

void transpile_block(FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu, Block* block, usize indent) {
    fprintf(code_stream, "({\n");
    list_foreach(&block->expressions, lambda(void, (Expression* expr) {
        fprint_indent(code_stream, indent + 1);
        transpile_expression(code_stream, modkey, func, fukey, fu, expr, indent + 1);
        fprintf(code_stream, ";\n", gen_c_type_name(block->res, fu));
    }));
    if (!block->yield_last) {
        fprint_indent(code_stream, indent + 1);
        fprintf(code_stream, "(%s) {};\n", gen_c_type_name(block->res, fu));
    }
    fprint_indent(code_stream, indent);
    fprintf(code_stream, "})");
}

void transpile_function_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu) {
    if (fu->generic_use != NULL) {
        map_foreach(fu->generic_use->generic_uses, lambda(void, (str inner_fukey, FuncUsage* inner_fu) {
            FuncUsage* fu_concrete = gc_malloc(sizeof(FuncUsage));
            fu_concrete->generic_use = inner_fu->generic_use;
            fu_concrete->generics = map_new();
            map_foreach(fu->generics, lambda(void, (str key, TypeValue* tv) {
                str valkey = to_str_writer(s, fprint_typevalue(s, tv));
                if (map_contains(inner_fu->generics, valkey)) {
                    TypeValue* replacement =  map_get(inner_fu->generics, valkey);
                    map_put(fu_concrete->generics, key, replacement);
                } else {
                    map_put(fu_concrete->generics, key, tv);
                }
            }));
            str concrete_fukey = funcusage_to_key(fu_concrete);
            if (!map_contains(func->generic_uses, concrete_fukey) && !map_contains(func->indirect_generics, concrete_fukey)) {
                map_put(func->indirect_generics, concrete_fukey, fu_concrete);
                transpile_function_generic_variant(header_stream, code_stream, modkey, func, concrete_fukey, fu_concrete);
            }
        }));
        return;
    }
    str c_fn_name = gen_c_fn_name(func, fukey);
    str c_ret_ty = gen_c_type_name(func->return_type, fu);

    fprintf(header_stream, "// %s::%s%s\n", modkey, func->name->name, fukey);
    fprintf(header_stream, "%s %s(", c_ret_ty, c_fn_name);
    list_foreach_i(&func->args, lambda(void, (usize i, Argument* arg) {
        str c_ty = gen_c_type_name(arg->type, fu);
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
        fprintf(code_stream, "// %s::%s%s\n", modkey, func->name->name, fukey);
        fprintf(code_stream, "%s %s(", c_ret_ty, c_fn_name);
        list_foreach_i(&func->args, lambda(void, (usize i, Argument* arg) {
            str c_ty = gen_c_type_name(arg->type, fu);
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
        transpile_block(code_stream, modkey, func, fukey, fu, func->body, 1);
        fprintf(code_stream, ";\n");
        fprintf(code_stream, "}\n");
        fprintf(code_stream, "\n");
    }
}

void transpile_function(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func) {
    func->indirect_generics = map_new();
    map_foreach(func->generic_uses, lambda(void, (str key, FuncUsage* fu) {
        transpile_function_generic_variant(header_stream, code_stream, modkey, func, key, fu);
    }));
}

void transpile_typedef_def(FILE* header_stream, FILE* code_stream, str modkey, TypeDef* ty) {
    if (!ty->extern_ref) {
        fprintf(header_stream, "// %s::%s\n", modkey, ty->name->name);
        fprintf(header_stream, "typedef struct %s %s;\n", ty->name->name, ty->name->name);
        fprintf(header_stream, "\n");
    }
}

void transpile_typedef_body(FILE* header_stream, FILE* code_stream, str modkey, TypeDef* ty) {
    if (!ty->extern_ref) {
        fprintf(header_stream, "// %s::%s\n", modkey, ty->name->name);
        fprintf(header_stream, "typedef struct %s {\n", ty->name->name);
        map_foreach(ty->fields, lambda(void, (str key, Field* f) {
            str c_ty = gen_c_type_name(f->type, NULL);
            fprintf(header_stream, "   %s %s;\n", c_ty, key);
        }));
        fprintf(header_stream, "} %s;\n", ty->name->name);
        fprintf(header_stream, "\n");
    }
}

void transpile_module(FILE* header_stream, FILE* code_stream, str modkey, Module* module,usize stage) {
    if (stage == 1) map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            break;
        case MIT_STRUCT:
            transpile_typedef_def(header_stream, code_stream, modkey, item->item);
            break;
        }
    }));
    if (stage == 2) map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            break;
        case MIT_STRUCT:
            transpile_typedef_body(header_stream, code_stream, modkey, item->item);
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
    FuncUsage* main_fu = map_get(main_func->generic_uses, "#"); // we know its this key
    fprintf(code_stream, "// c main entrypoint\n");
    fprintf(code_stream, "int main(int argc, char** argv) {\n");
    fprintf(code_stream, "    __global__argc = argc;\n");
    fprintf(code_stream, "    __global__argv = argv;\n");
    fprintf(code_stream, "    %s();\n", gen_c_fn_name(main_func, funcusage_to_key(main_fu)));
    fprintf(code_stream, "    return 0;\n");
    fprintf(code_stream, "}\n");
}
