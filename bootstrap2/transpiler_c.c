#include <stdlib.h>
#include <stdio.h>
#include "transpiler_c.h"
#include "ast.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
#include "module.h"
#include "parser.h"
#include "token.h"

str gen_c_fn_name(FuncDef* def, FuncUsage* fu) {
    if (def->body == NULL) {
        if (map_size(fu->generics) > 0) spanned_error("Generic extern function", def->name->span, "Extern functions may not be generic");
        return def->name->name;
    }
    return to_str_writer(stream, fprintf(stream,"%s%p", def->name->name, fu));
}

str gen_temp_c_name() {
    return to_str_writer(stream, fprintf(stream, "temp%lu", random()));
}

str gen_c_type_name(TypeValue* tv, FuncUsage* fu) {
    if (tv->def->extern_ref != NULL) {
        if (tv->generics.elements > 0) spanned_error("Generic extern type", tv->def->name->span, "Extern types may not be generic");
        return tv->def->extern_ref;
    }
    if (tv->generics.elements > 0) todo("Generic types");
    return to_str_writer(stream, fprintf(stream,"struct %s", tv->def->name->name));
}

void print_indent(FILE* file, usize indent) {
    for (usize i = 0;i < indent;i++) {
        fprintf(file, "    ");
    }
}

void transpile_block(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu, Block* block, usize indent);
void transpile_expression(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu, Expression* expr, usize indent) {
    switch (expr->type) {
        case EXPR_BIN_OP: {
            todo("EXPR_BIN_OP");
        } break;
        case EXPR_FUNC_CALL: {
            FuncCall* fc = expr->expr;
            str c_fn_name = gen_c_fn_name(fc->def, fu);
            fprintf(code_stream, "%s(", c_fn_name);
            list_foreach_i(&fc->arguments, lambda(void, (usize i, Expression* arg) {
                transpile_expression(header_stream, code_stream, modkey, func, fukey, fu, arg, indent + 1);
                if (i > 0) fprintf(code_stream, ",");
            }));
            fprintf(code_stream, ")");
        } break;
        case EXPR_LITERAL: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING:
                    fprint_token(code_stream, lit);
                    break;
                case NUMERAL:
                    fprint_token(code_stream, lit);
                    break;
                default:
                    unreachable("%s", TokenType__NAMES[lit->type]);
            }
        } break;
        case EXPR_BLOCK: {
            transpile_block(header_stream, code_stream, modkey, func, fukey, fu, expr->expr, 1);
        } break;
        case EXPR_VARIABLE: {
            todo("EXPR_VARIABLE");
        } break;
        case EXPR_LET: {
            todo("EXPR_LET");
        } break;
        case EXPR_CONDITIONAL: {
            todo("EXPR_CONDITIONAL");
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
    }
}

void transpile_block(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu, Block* block, usize indent) {
    fprintf(code_stream, "({\n");
    list_foreach(&block->expressions, lambda(void, (Expression* expr) {
        print_indent(code_stream, indent + 1);
        transpile_expression(header_stream, code_stream, modkey, func, fukey, fu, expr, indent + 1);
        fprintf(code_stream, ";\n", gen_c_type_name(block->res, fu));
    }));
    if (!block->yield_last) {
        print_indent(code_stream, indent + 1);
        fprintf(code_stream, "(%s) {};\n", gen_c_type_name(block->res, fu));
    }
    print_indent(code_stream, indent);
    fprintf(code_stream, "})");
}

void transpile_function_generic_variant(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func, str fukey, FuncUsage* fu) {
    log("   %s%s", func->name->name, fukey);
    str c_fn_name = gen_c_fn_name(func, fu);
    str c_ret_ty = gen_c_type_name(func->return_type, fu);

    fprintf(header_stream, "// %s::%s%s\n", modkey, func->name->name, fukey);
    fprintf(header_stream, "%s %s();\n", c_ret_ty, c_fn_name);
    fprintf(header_stream, "\n");

    if (func->body != NULL) {
        fprintf(code_stream, "// %s::%s%s\n", modkey, func->name->name, fukey);
        fprintf(code_stream, "%s %s() {\n", c_ret_ty, c_fn_name);
        fprintf(code_stream, "    return ");
        transpile_block(header_stream, code_stream, modkey, func, fukey, fu, func->body, 1);
        fprintf(code_stream, ";\n");
        fprintf(code_stream, "}\n");
        fprintf(code_stream, "\n");
    }
}

void transpile_function(FILE* header_stream, FILE* code_stream, str modkey, FuncDef* func) {
    log("transpiling function %s", func->name->name);
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
        fprintf(header_stream, "} %s;\n", ty->name->name);
        fprintf(header_stream, "\n");
    }
}

void transpile_module(FILE* header_stream, FILE* code_stream, str modkey, Module* module) {
    log("transpiling module %s", modkey);
    map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            break;
        case MIT_STRUCT:
            transpile_typedef_def(header_stream, code_stream, modkey, item->item);
            break;
        }
    }));
    map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
        switch (item->type) {
        case MIT_FUNCTION:
            break;
        case MIT_STRUCT:
            transpile_typedef_body(header_stream, code_stream, modkey, item->item);
            break;
        }
    }));
    map_foreach(module->items, lambda(void, (str key, ModuleItem* item) {
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

    fprintf(code_stream, "/* File generated by kommando compiler. Do not modify. */\n");
    fprintf(code_stream, "#include \"%s\"\n", header_name);
    fprintf(code_stream, "\n");

    map_foreach(program->modules, lambda(void, (str key, Module* mod) {
        transpile_module(header_stream, code_stream, key, mod);
    }));

    ModuleItem* main_func_item = map_get(program->main_module->items, "main");
    FuncDef* main_func = main_func_item->item;
    FuncUsage* main_fu = map_get(main_func->generic_uses, "#"); // we know its this key
    fprintf(code_stream, "// c main entrypoint\nint main(int argc, char** argv) {\n    main%p();\n    return 0;\n}\n", main_fu);
}
