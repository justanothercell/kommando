#pragma once

#include "ast.c"
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void write_code(FILE* dest, const char* format, ...) {
    va_list argptr;
    va_start(argptr, format);
    vfprintf(dest, format, argptr);
    va_end(argptr);
}

void write_indent(FILE* dest, usize indent) {
    for (int i = 0;i < indent;i++) {
        write_code(dest, "    ");
    }
}

typedef struct Writer {
    FILE* code;
    FILE* header;
} Writer;

Writer* new_writer(char* name, FILE* code, FILE* header) {
    Writer* writer = malloc(sizeof(Writer));
    writer->code = code;
    writer->header = header;
    write_code(code, "#pragma once\n\n#include <stdio.h>\n\n#include \"%s.h\"\n\n", name);
    write_code(code, "int main(int arg_c, char* arg_v[]) {\n    __entry__();\n    return 0;\n}\n\n", name);
    write_code(header, "#pragma once\n\n", name);
    return writer;
}

void drop_writer(Writer* writer) {
    fclose(writer->code);
    fclose(writer->header);
    free(writer);
}

void transpile_block(Writer *writer, Block* block, usize indent);

void transpile_expression(Writer* writer, Expression* expr, usize indent) {
    // write_code(writer->code, "/* %d */", expr->type);
    switch (expr->type) {
        case FUNC_CALL_EXPR:
            {
                FunctionCall* call = expr->expr;
                write_code(writer->code, "%s(", call->name);
                for (usize i = 0;i < call->args_c;i++) {
                    if (i > 0) write_code(writer->code, ", ");
                    transpile_expression(writer, call->args[i], indent);
                }
                write_code(writer->code, ")");
            }
            break;
        case BLOCK_EXPR:
            {
                Block* block = expr->expr;
                write_code(writer->code, "{/* TODO: BLOCK */}");
            }
            break;
        case IF_EXPR:
            {
                IfExpr* if_expr = expr->expr;
                write_code(writer->code, "if (");
                transpile_expression(writer, if_expr->condition, indent);
                write_code(writer->code, ") ");
                transpile_block(writer, if_expr->then, indent);
                if (if_expr->otherwise != NULL) {
                    write_code(writer->code, " else ");
                    transpile_block(writer, if_expr->otherwise, indent);
                }
            }
            break;
        case WHILE_EXPR:
            {
                WhileExpr* while_expr = expr->expr;
                write_code(writer->code, "while (");
                transpile_expression(writer, while_expr->condition, indent);
                write_code(writer->code, ") ");
                transpile_block(writer, while_expr->body, indent);
            }
            break;
        case LITERAL_EXPR:
            {
                Token* token = expr->expr;
                switch (token->type) {
                    case STRING:
                        write_code(writer->code, "\"%s\"", token->string);
                        break;
                    case NUMERAL:
                        write_code(writer->code, "%s", token->string);
                        break;
                    case IDENTIFIER:
                    case SNOWFLAKE:
                        fprintf(stderr, "Unreachable case: %s as literal", TOKENTYPE_STRING[token->type]);
                        exit(1);
                        break;
                }
            }
            break;
        case VARIABLE_EXPR:
            {
                char* name = expr->expr;
                write_code(writer->code, name);
            }
            break;
    }
}

void transpile_block(Writer *writer, Block* block, usize indent) {
    write_code(writer->code, "{\n");
    for (usize i = 0;i < block->expr_c;i++) {
        write_indent(writer->code, indent + 1);
        transpile_expression(writer, block->exprs[i], indent + 1);
        write_code(writer->code, ";\n");
    }
    write_indent(writer->code, indent);
    write_code(writer->code, "}");
}

void transpile_module(Writer* writer, Module* module) {
    for (usize i = 0;i < module->funcs_c;i++) {
        FunctionDef* func = module->funcs[i];
        Type* ret_t = find_type(module, func->ret_t);
        write_type(module, writer->header, ret_t);
        write_type(module, writer->code, ret_t);
        write_code(writer->header, " %s(", func->name);
        write_code(writer->code, " %s(", func->name);
        if (func->args_c == 0) {
            write_code(writer->header, "void");
            write_code(writer->code, "void");
        } else {
            for (usize i = 0;i < func->args_c;i++) {
                if (i > 0) {
                    write_code(writer->header, ", ");
                    write_code(writer->code, ", ");
                }
                Type* arg_t = find_type(module, func->args_t[i]);
                write_type(module, writer->header, arg_t);
                write_type(module, writer->code, arg_t);
                write_code(writer->header, " %s", func->args[i]);
                write_code(writer->code, " %s", func->args[i]);
            }
        }
        write_code(writer->header, ");\n");
        write_code(writer->code, ") ");
        transpile_block(writer, func->body, 0);
        write_code(writer->code, "\n\n");
    }
}
