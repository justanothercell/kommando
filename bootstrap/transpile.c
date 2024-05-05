#include "transpile.h"

#include "ast.h"
#include "types.h"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void write_code(FILE* dest, const str format, ...) {
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

Writer* new_writer(str name, FILE* code, FILE* header, bool gen_main) {
    Writer* writer = malloc(sizeof(Writer));
    writer->code = code;
    writer->header = header;
    writer->gen_main = gen_main;

    write_code(header, "#pragma once\n\n#define __offset(a, b) ((a)+(b))\n\n");
    write_code(code, "#include <stdio.h>\n#include <stdlib.h>\n\n#include \"%s.h\"\n\n", name);
    if (gen_main) {
    write_code(header, "#define main __entry__\n\n", name);
    write_code(code, "#define main __entry__\n\n", name);
    }
    return writer;
}

void drop_writer(Writer* writer) {
    fclose(writer->code);
    fclose(writer->header);
    free(writer);
}

void finalize_transpile(Writer* writer) {
    if (writer->gen_main) {
        write_code(writer->code, "\n\n#undef main\n\nint main(int arg_c, char* arg_v[]) {\n    __entry__();\n    return 0;\n}\n\n");
    }
}

void transpile_expression(Module* module, Writer* writer, Expression* expr, usize indent) {
    // write_code(writer->code, "/* %d */", expr->type);
    switch (expr->kind) {
        case FUNC_CALL_EXPR:
            {
                FunctionCall* call = expr->expr;
                write_code(writer->code, "%s(", call->name);
                for (usize i = 0;i < call->args.length;i++) {
                    if (i > 0) write_code(writer->code, ", ");
                    transpile_expression(module, writer, call->args.elements[i], indent);
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
                str name = expr->expr;
                write_code(writer->code, name);
            }
            break;
        case REF_EXPR:
        case DEREF_EXPR:
        case RETURN_EXPR:
            {
                Expression* inner = expr->expr;
                switch (expr->kind) {
                    case REF_EXPR:
                        write_code(writer->code, "(&");
                        break;
                    case DEREF_EXPR:
                        write_code(writer->code, "(*");
                        break;
                    case RETURN_EXPR:
                        write_code(writer->code, "return ");
                        break;
                    default:
                        fprintf(stderr, "Unreachabe case reached");
                }
                transpile_expression(module, writer, inner, indent);
                switch (expr->kind) {
                    case REF_EXPR:
                    case DEREF_EXPR:
                        write_code(writer->code, ")");
                    default:
                        break;
                }
            }
            break;
        case STRUCT_LITERAL:
            {
                str s = expr->expr;
                write_code(writer->code, "(struct %s) { }", s);
            }
            break;
        case VAR_DECL_EXPR:
            {
                VarDecl* vd = expr->expr;
                write_type(module, writer->code, find_type(module, vd->type, expr->src_line));
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
        case FIELD_ACCESS_EXPR:
            {
                FieldAccess* fa = expr->expr;
                transpile_expression(module, writer, fa->object, indent + 1);
                if (fa->object->type[0] == '&') write_code(writer->code, "->");
                else write_code(writer->code, ".");
                transpile_expression(module, writer, fa->field, indent + 1);
            }
            break;
        case BINOP_EXPR:
            {
                BinOp* op = expr->expr;
                write_code(writer->code, "(");
                transpile_expression(module, writer, op->lhs, indent + 1);
                write_code(writer->code, " %s ", op->op);
                transpile_expression(module, writer, op->rhs, indent + 1);
                write_code(writer->code, ")");
            }
            break;
    }
}

void transpile_block(Module* module, Writer *writer, Block* block, usize indent) {
    write_code(writer->code, "{\n");
    for (usize i = 0;i < block->exprs.length;i++) {
        write_indent(writer->code, indent + 1);
        transpile_expression(module, writer, block->exprs.elements[i], indent + 1);
        write_code(writer->code, ";\n");
    }
    write_indent(writer->code, indent);
    write_code(writer->code, "}");
}

void transpile_function(Writer* writer, Module* module, FunctionDef* func) {
    Type* ret_t = find_type(module, func->ret_t, func->src_line);
    write_type(module, writer->header, ret_t);
    write_type(module, writer->code, ret_t);
    write_code(writer->header, " %s(", func->name);
    write_code(writer->code, " %s(", func->name);
    if (func->args.length == 0) {
        write_code(writer->header, "void");
        write_code(writer->code, "void");
    } else {
        for (usize i = 0;i < func->args.length;i++) {
            if (i > 0) {
                write_code(writer->header, ", ");
                write_code(writer->code, ", ");
            }
            Type* arg = find_type(module, func->args_t.elements[i], func->src_line);
            write_type(module, writer->header, arg);
            write_type(module, writer->code, arg);
            write_code(writer->header, " %s", func->args.elements[i]);
            write_code(writer->code, " %s", func->args.elements[i]);
        }
    }
    write_code(writer->header, ");\n");
    write_code(writer->code, ") ");
    transpile_block(module, writer, func->body, 0);
}

void transpile_struct_def(Writer* writer, Module* module, str name, Struct* s) {
    write_code(writer->header, "struct %s;\n", name);
    if (s->fields.length > 0) {
        write_code(writer->code, "struct %s {\n", name);
        for (usize j = 0;j < s->fields.length;j++) {
            write_code(writer->code, "    ");
            write_type(module, writer->code, find_type(module, s->fields_t.elements[j], s->src_line));
            write_code(writer->code, " %s;\n", s->fields.elements[j]);
        }
        write_code(writer->code, "};\n", name);
    } else {
        write_code(writer->code, "struct %s {};\n", name);
    }
}

void transpile_module(Writer* writer, Module* module) {
    for (usize i = 0;i < module->types.length;i++) {
        TypeDef* tdef = module->types.elements[i];
        if (tdef->type->type == TYPE_STRUCT) {
            Struct* s = (Struct*)tdef->type->ty;
            transpile_struct_def(writer, module, tdef->name, s);
            drop_temp_types();
        }
    }

    write_code(writer->header, "\n");
    write_code(writer->code, "\n");

    for (usize i = 0;i < module->funcs.length;i++) {
        FunctionDef* func = module->funcs.elements[i];
        if (func->body != NULL) {
            transpile_function(writer, module, func);
            write_code(writer->code, "\n\n");
            drop_temp_types();
        }
    }
}
