#pragma once

#include "ast.c"
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    write_code(header, "#pragma once\n\n", name);
    write_code(code, "#include <stdio.h>\n\n#include \"%s.h\"\n\n", name);
    
    return writer;
}

void drop_writer(Writer* writer) {
    fclose(writer->code);
    fclose(writer->header);
    free(writer);
}

void finalize_transpile(Writer* writer) {
    write_code(writer->code, "\nint main(int arg_c, char* arg_v[]) {\n    __entry__();\n    return 0;\n}\n\n");
}

void transpile_block(Module* module, Writer *writer, Block* block, usize indent);

void transpile_expression(Module* module, Writer* writer, Expression* expr, usize indent) {
    // write_code(writer->code, "/* %d */", expr->type);
    switch (expr->type) {
        case FUNC_CALL_EXPR:
            {
                FunctionCall* call = expr->expr;
                write_code(writer->code, "%s(", call->name);
                for (usize i = 0;i < call->args_c;i++) {
                    if (i > 0) write_code(writer->code, ", ");
                    transpile_expression(module, writer, call->args[i], indent);
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
                transpile_expression(module, writer, if_expr->condition, indent);
                write_code(writer->code, ") ");
                transpile_block(module, writer, if_expr->then, indent);
                if (if_expr->otherwise != NULL) {
                    write_code(writer->code, " else ");
                    transpile_block(module, writer, if_expr->otherwise, indent);
                }
            }
            break;
        case WHILE_EXPR:
            {
                WhileExpr* while_expr = expr->expr;
                write_code(writer->code, "while (");
                transpile_expression(module, writer, while_expr->condition, indent);
                write_code(writer->code, ") ");
                transpile_block(module, writer, while_expr->body, indent);
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
        case REF_EXPR:
        case DEREF_EXPR:
        case RETURN_EXPR:
            {
                Expression* inner = expr->expr;
                switch (expr->type) {
                    case REF_EXPR:
                        write_code(writer->code, "&");
                        break;
                    case DEREF_EXPR:
                        write_code(writer->code, "*");
                        break;
                    case RETURN_EXPR:
                        write_code(writer->code, "return ");
                        break;
                    default:
                        fprintf(stderr, "Unreachabe case reached");
                }
                transpile_expression(module, writer, inner, indent);
            }
            break;
        case STRUCT_LITERAL:
            {
                char* s = expr->expr;
                write_code(writer->code, "(struct %s) { }", s);
            }
            break;
        case VAR_DECL_EXPR:
            {
                VarDecl* vd = expr->expr;
                write_type(module, writer->code, find_type(module, vd->type));
                write_code(writer->code, " %s", vd->name);
                if (vd->value != NULL) {
                    write_code(writer->code, " = ");
                    transpile_expression(module, writer, vd->value, indent);
                }
            }
            break;
        case ASSIGN_EXPR:
            {
                Assign* assign = expr->expr;
                transpile_expression(module, writer, assign->target, indent + 1);
                write_code(writer->code, " = ");
                transpile_expression(module, writer, assign->value, indent + 1);
            }
            break;
    }
}

void transpile_block(Module* module, Writer *writer, Block* block, usize indent) {
    write_code(writer->code, "{\n");
    for (usize i = 0;i < block->expr_c;i++) {
        write_indent(writer->code, indent + 1);
        transpile_expression(module, writer, block->exprs[i], indent + 1);
        write_code(writer->code, ";\n");
    }
    write_indent(writer->code, indent);
    write_code(writer->code, "}");
}

void transpile_function(Writer* writer, Module* module, FunctionDef* func) {
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
            Type* arg = find_type(module, func->args_t[i]);
            write_type(module, writer->header, arg);
            write_type(module, writer->code, arg);
            write_code(writer->header, " %s", func->args[i]);
            write_code(writer->code, " %s", func->args[i]);
        }
    }
    write_code(writer->header, ");\n");
    write_code(writer->code, ") ");
    transpile_block(module, writer, func->body, 0);
}

void transpile_struct_def(Writer* writer, Module* module, char* name, Struct* s) {
    write_code(writer->header, "struct %s;\n", name);
    if (s->fields_c > 0) {
        write_code(writer->code, "struct %s {\n", name);
        for (usize j = 0;j < s->fields_c;j++) {
            write_code(writer->code, "    ");
            write_type(module, writer->code, find_type(module, s->fields_t[j]));
            write_code(writer->code, " %s;\n", s->fields[j]);
        }
        write_code(writer->code, "};\n", name);
    } else {
        write_code(writer->code, "struct %s {};\n", name);
    }
}

void transpile_module(Writer* writer, Module* module) {
    for (usize i = 0;i < module->types_c;i++) {
        TypeDef* tdef = module->types[i];
        if (tdef->type->type == TYPE_STRUCT) {
            Struct* s = (Struct*)tdef->type->ty;
            transpile_struct_def(writer, module, tdef->name, s);
        }
    }

    write_code(writer->header, "\n");
    write_code(writer->code, "\n");

    for (usize i = 0;i < module->funcs_c;i++) {
        FunctionDef* func = module->funcs[i];
        transpile_function(writer, module, func);
        
        write_code(writer->code, "\n\n");
    }
}
