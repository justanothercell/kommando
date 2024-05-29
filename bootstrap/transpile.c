#include "transpile.h"

#include "lib/defines.h"

#include "ast.h"
#include "types.h"
#include "infer.h"

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

    write_code(header, "#define __index(a, b) (*((a)+(b)))\n\n");
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

void transpile_expression(Module* module, Writer* writer, Expression* expr, GenericsMapping* mapping, usize indent) {
    // write_code(writer->code, "/* %d */", expr->type);
    switch (expr->kind) {
        case FUNC_CALL_EXPR:
            {
                FunctionCall* call = expr->expr;
                write_func(module, writer->code, find_func_def(module, call, expr->src_line), mapping);
                write_code(writer->code, "(");
                for (usize i = 0;i < call->args.length;i++) {
                    if (i > 0) write_code(writer->code, ", ");
                    transpile_expression(module, writer, call->args.elements[i], mapping, indent);
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
                transpile_expression(module, writer, if_expr->condition, mapping, indent);
                write_code(writer->code, ") ");
                transpile_block(module, writer, if_expr->then, mapping, indent);
                if (if_expr->otherwise != NULL) {
                    write_code(writer->code, " else ");
                    transpile_block(module, writer, if_expr->otherwise, mapping, indent);
                }
            }
            break;
        case WHILE_EXPR:
            {
                WhileExpr* while_expr = expr->expr;
                write_code(writer->code, "while (");
                transpile_expression(module, writer, while_expr->condition, mapping, indent);
                write_code(writer->code, ") ");
                transpile_block(module, writer, while_expr->body, mapping, indent);
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
                transpile_expression(module, writer, inner, mapping, indent);
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
                TypeValue* ty_resolved = degenerify_mapping(vd->type, mapping, vd->type->src_line);
                write_type(module, writer->code, find_type_def(module, ty_resolved), vd->type->indirection, &ty_resolved->generics);
                write_code(writer->code, " %s", vd->name);
                if (vd->value != NULL) {
                    write_code(writer->code, " = ");
                    transpile_expression(module, writer, vd->value, mapping, indent);
                }
            }
            break;
        case ASSIGN_EXPR:
            {
                Assign* assign = expr->expr;
                transpile_expression(module, writer, assign->target, mapping, indent + 1);
                write_code(writer->code, " = ");
                transpile_expression(module, writer, assign->value, mapping, indent + 1);
            }
            break;
        case FIELD_ACCESS_EXPR:
            {
                FieldAccess* fa = expr->expr;
                transpile_expression(module, writer, fa->object, mapping, indent + 1);
                if (fa->object->type->indirection > 0) write_code(writer->code, "->");
                else write_code(writer->code, ".");
                transpile_expression(module, writer, fa->field, mapping, indent + 1);
            }
            break;
        case BINOP_EXPR:
            {
                BinOp* op = expr->expr;
                write_code(writer->code, "(");
                transpile_expression(module, writer, op->lhs, mapping, indent + 1);
                write_code(writer->code, " %s ", op->op);
                transpile_expression(module, writer, op->rhs, mapping, indent + 1);
                write_code(writer->code, ")");
            }
            break;
    }
}

void transpile_block(Module* module, Writer *writer, Block* block, GenericsMapping* mapping, usize indent) {
    write_code(writer->code, "{\n");
    for (usize i = 0;i < block->exprs.length;i++) {
        write_indent(writer->code, indent + 1);
        transpile_expression(module, writer, block->exprs.elements[i], mapping, indent + 1);
        write_code(writer->code, ";\n");
    }
    write_indent(writer->code, indent);
    write_code(writer->code, "}");
}

void transpile_function(Writer* writer, Module* module, FunctionDef* func, GenericsMapping* mapping) {
    TypeValue* ret_t_resolved = degenerify_mapping(func->ret_t, mapping, func->ret_t->src_line);
    TypeDef* ret_t = find_type_def(module, ret_t_resolved);
    write_type(module, writer->header, ret_t, func->ret_t->indirection, &ret_t_resolved->generics);
    write_type(module, writer->code, ret_t, func->ret_t->indirection, &ret_t_resolved->generics);
    write_code(writer->header, " ");
    write_code(writer->code, " ");
    write_func(module, writer->header, func, mapping);
    write_func(module, writer->code, func, mapping);
    write_code(writer->header, "(");
    write_code(writer->code, "(");
    if (func->args.length == 0) {
        write_code(writer->header, "void");
        write_code(writer->code, "void");
    } else {
        for (usize i = 0;i < func->args.length;i++) {
            if (i > 0) {
                write_code(writer->header, ", ");
                write_code(writer->code, ", ");
            }
            TypeValue* argtv = func->args_t.elements[i];
            TypeDef* arg = find_type_def(module, degenerify_mapping(argtv, mapping, argtv->src_line));
            write_type(module, writer->header, arg, argtv->indirection, &argtv->generics);
            write_type(module, writer->code, arg, argtv->indirection, &argtv->generics);
            write_code(writer->header, " %s", func->args.elements[i]);
            write_code(writer->code, " %s", func->args.elements[i]);
        }
    }
    write_code(writer->header, ");\n");
    write_code(writer->code, ") ");
    transpile_block(module, writer, func->body, mapping, 0);
}

void transpile_type_def(Writer* writer, Module* module, TypeDef* type) {
    if (type->type->type != TYPE_STRUCT) {
        //fprintf(stderr, "WARNING: tried to register nonstruct type: Type `%s` is not a struct\n", type->name);
        return;
    }
    Struct* s = type->type->ty;
    if (type->generics.length == 0) { // type is not generic
        printf("    transpiling ");
        fprint_type(stdout, type); 
        printf("\n");
        write_type(module, writer->header, type, 0, NULL);
        if (s->fields.length > 0) {
            write_code(writer->header, " {\n");
            for (usize j = 0;j < s->fields.length;j++) {
                write_code(writer->header, "    ");
                write_type(module, writer->header, find_type_def(module, s->fields_t.elements[j]), s->fields_t.elements[j]->indirection, &s->fields_t.elements[j]->generics);
                write_code(writer->header, " %s;\n", s->fields.elements[j]);
            }
            write_code(writer->header, "};\n");
        } else {
            write_code(writer->header, " {};\n");
        }
    } else {
        printf("    transpiling ");
        fprint_type(stdout, type); 
        printf(" with %lld variants\n", type->mappings.length);
        for (usize i = 0;i < type->mappings.length;i++) {
            GenericsMapping mapping = type->mappings.elements[i];

            TypeValueList tvlist = list_new();

            for (usize j = 0;j < mapping.length;j++) {
                list_append(&tvlist, mapping.elements[j]->type);
            }
            write_type(module, writer->header, type, 0, &tvlist);
            free(tvlist.elements);

            if (s->fields.length > 0) {
                write_code(writer->header, " {\n");
                for (usize j = 0;j < s->fields.length;j++) {
                    write_code(writer->header, "    ");
                    TypeValue* field_tv = s->fields_t.elements[j];
                    TypeValue* resolved_field_tv = degenerify(field_tv, &type->generics, &tvlist, field_tv->src_line);
                    TypeValueList field_generics = list_new();
                    for (usize k = 0;k < field_tv->generics.length;k++) {
                        list_append(&field_generics, degenerify(field_tv->generics.elements[k], &type->generics, &tvlist, field_tv->generics.elements[k]->src_line));
                    }
                    write_type(module, writer->header, find_type_def(module, resolved_field_tv), field_tv->indirection, &field_generics);
                    free(field_generics.elements);
                    write_code(writer->header, " %s;\n", s->fields.elements[j]);
                }
                write_code(writer->header, "};\n");
            } else {
                write_code(writer->header, " {};\n");
            }
            fflush(writer->header);
        }
    }
}

void transpile_module(Writer* writer, Module* module) {
    for (usize i = 0;i < module->types.length;i++) {
        transpile_type_def(writer, module, module->types.elements[i]);
        fflush(writer->header);
        fflush(writer->code);
    }

    write_code(writer->header, "\n");
    write_code(writer->code, "\n");

    for (usize i = 0;i < module->funcs.length;i++) {
        FunctionDef* func = module->funcs.elements[i];
        if (func->body != NULL) {
            if (func->generics.length == 0) {
                printf("    transpiling ");
                fprint_func(stdout, func); 
                printf("\n");
                transpile_function(writer, module, func, NULL);
                write_code(writer->code, "\n\n");
                fflush(writer->header);
                fflush(writer->code);
            } else {
                printf("    transpiling ");
                fprint_func(stdout, func); 
                printf(" with %lld variants\n", func->mappings.length);
                for (usize j = 0;j < func->mappings.length;j++) {
                    transpile_function(writer, module, func, &func->mappings.elements[j]);
                    write_code(writer->code, "\n\n");
                    fflush(writer->header);
                    fflush(writer->code);
                }
            }
        }
    }
}
