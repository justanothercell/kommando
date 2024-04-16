#pragma once

#include "ast.c"
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <vadefs.h>

void write_code(FILE* dest, const char* format, ...) {
    va_list argptr;
    va_start(argptr, format);
    vfprintf(dest, format, argptr);
    va_end(argptr);
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

void transpile_expression(Writer* writer, Expression* expr) {
    switch (expr->type) {
        case FUNC_CALL_EXPR:
            {
                FunctionCall* call = expr->expr;
                write_code(writer->code, "%s(", call->name);
                for (usize i = 0;i < call->args_c;i++) {
                    if (i > 0) write_code(writer->code, ", ");
                    transpile_expression(writer, call->args[i]);
                }
                write_code(writer->code, ")");
            }
            break;
        case BLOCK_EXPR:
            {
                Block* block = expr->expr;
                write_code(writer->code, "{/* TODO */}");
            }
            break;
        case IF_EXPR:
            {
                // TODO
            }
            break;
        case WHILE_EXPR:
            {
                // TODO
            }
            break;
        case LITERAL_EXPR:
            {
                Token* token = expr->expr;
                switch (token->type) {
                    case STRING:
                        write_code(writer->code, "\"%s\"", token->string);
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

void transpile_module(Writer* writer, Module* module) {
    for (usize i = 0;i < module->funcs_c;i++) {
        FunctionDef* func = module->funcs[i];
        write_code(writer->header, "%s %s(", func->ret_t->name, func->name);
        write_code(writer->code, "%s %s(", func->ret_t->name, func->name);
        if (func->arg_c == 0) {
            write_code(writer->header, "void");
            write_code(writer->code, "void");
        } else {
            for (usize i = 0;i < func->arg_c;i++) {
                if (i > 0) {
                    write_code(writer->header, ", ");
                    write_code(writer->code, ", ");
                }
                write_code(writer->header, "%s %s", func->args_t[i]->name, func->args[i]);
                write_code(writer->code, "%s %s", func->args_t[i]->name, func->args[i]);
            }
        }
        write_code(writer->header, ");\n");
        write_code(writer->code, ") {\n");
        for (usize i = 0;i < func->body->expr_c;i++) {
            write_code(writer->code, "    "); 
            transpile_expression(writer, func->body->exprs[i]);    
            write_code(writer->code, ";\n");
        }
        write_code(writer->code, "}\n\n");
    }
}
