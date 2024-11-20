#include "parser.h"
#include "ast.h"
#include "lib.h"
#include "lib/defines.h"
#include "lib/exit.h"
#include "lib/list.h"
#include "lib/str.h"
#include "module.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>

bool double_double_colon(TokenStream* stream) {
    Token* t = try_next_token(stream);
    if (t == NULL) return false;
    if (!token_compare(t, ":", SNOWFLAKE)) {
        stream->peek = t;
        return false;
    }
    t = next_token(stream);
    if (!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t);
    return true;
}

TypeDef* parse_struct(TokenStream* stream) {
    Token* t = next_token(stream);
    if (!token_compare(t, "struct", IDENTIFIER)) unexpected_token(t);
    Identifier* name = parse_identifier(stream);
    Map* fields = map_new();
    IdentList flist = list_new(IdentList);
    t = next_token(stream);
    GenericKeys* keys = NULL;
    if (token_compare(t, "<", SNOWFLAKE)) {
        stream->peek = t;
        keys = parse_generic_keys(stream);
        t = next_token(stream);
    }
    if (!token_compare(t, "{", SNOWFLAKE)) unexpected_token(t);
    while (1) {
        t = next_token(stream);
        if (token_compare(t, "}", SNOWFLAKE)) break;
        stream->peek = t;
        Identifier* field_name = parse_identifier(stream);
        t = next_token(stream);
        if (!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t);
        TypeValue* tv = parse_type_value(stream);
        if (map_contains(fields, field_name->name)) spanned_error("Duplicate field name", field_name->span, "field with name %s already exists in struct %s", field_name->name, name->name);
        Field* field = malloc(sizeof(Field));
        field->name = name;
        field->type = tv;
        map_put(fields, field_name->name, field);
        list_append(&flist, field_name);
        t = next_token(stream);
        if (token_compare(t, "}", SNOWFLAKE)) break;
        if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
    }
    TypeDef* type = malloc(sizeof(TypeDef));
    type->extern_ref = NULL;
    type->generics = keys;
    type->flist = flist;
    type->fields = fields;
    type->transpile_state = 0;
    type->name = name;
    type->head_resolved = false;
    return type;
}

Module* parse_module_contents(TokenStream* stream, Path* path) {
    Module* module = malloc(sizeof(Module));
    module->imports = list_new(ImportList);
    module->items = map_new();
    module->path = path;
    module->resolved = false;
    module->in_resolution = false;
    module->filepath = NULL;
    module->subs = list_new(ModDefList);

    while (has_next(stream)) {
        Token* t = next_token(stream);
        stream->peek = t;

        bool pub = false;
        if (token_compare(t, "pub", IDENTIFIER)) {
            pub = true;
            t = next_token(stream); // skip peek 
            t = next_token(stream); // setting new peek
            stream->peek = t;
        }
        if (token_compare(t, "fn", IDENTIFIER)) {
            FuncDef* function = parse_function_definition(stream);
            ModuleItem* mi = malloc(sizeof(ModuleItem));
            mi->item = function;
            mi->type = MIT_FUNCTION;
            mi->module = module;
            mi->pub = pub;
            function->module = module;
            ModuleItem* old = map_put(module->items, function->name->name, mi);
            if (old != NULL) {
                Span span;
                switch (old->type) {
                    case MIT_FUNCTION:
                       span = ((FuncDef*)old->item)->name->span;
                       break;
                    case MIT_STRUCT:
                       span = ((TypeDef*)old->item)->name->span;
                       break;
                    case MIT_MODULE:
                       span = ((Module*)old->item)->name->span;
                       break;
                    case MIT_ANY: 
                        unreachable();
                }
                spanned_error("Name conflict", function->name->span, "Name %s is already defined in this scope at %s", function->name->name, to_str_writer(s, fprint_span(s, &span)));
            }
        } else if (token_compare(t, "struct", IDENTIFIER)) {
            TypeDef* type = parse_struct(stream);
            ModuleItem* mi = malloc(sizeof(ModuleItem));
            mi->item = type;
            mi->type = MIT_STRUCT;
            mi->module = module;
            mi->pub = pub;
            type->module = module;
            ModuleItem* old = map_put(module->items, type->name->name, mi);
            if (old != NULL) {
                Span span;
                switch (old->type) {
                    case MIT_FUNCTION:
                       span = ((FuncDef*)old->item)->name->span;
                       break;
                    case MIT_STRUCT:
                       span = ((TypeDef*)old->item)->name->span;
                       break;
                    case MIT_MODULE:
                       span = ((Module*)old->item)->name->span;
                       break;
                    case MIT_ANY: 
                        unreachable();
                }
                spanned_error("Name conflict", type->name->span, "Name %s is already defined in this scope at %s", type->name->name, to_str_writer(s, fprint_span(s, &span)));
            }
        } else if (token_compare(t, "mod", IDENTIFIER)) {
            t = next_token(stream); // skip mod
            Identifier* name = parse_identifier(stream);
            t = next_token(stream);
            if (!token_compare(t, ";", SNOWFLAKE)) unexpected_token(t);
            ModDef* mod = malloc(sizeof(ModDef));
            mod->name = name;
            mod->pub = pub;
            list_append(&module->subs, mod);
        } else if (token_compare(t, "use", IDENTIFIER)) {
            t = next_token(stream); // skip use
            Path* path = parse_path(stream);
            bool wildcard = false;
            if (path->ends_in_double_colon) {
                t = next_token(stream);
                if (token_compare(t, "*", SNOWFLAKE)) wildcard = true;
                else stream->peek = t;

            }
            t = next_token(stream);
            if (!token_compare(t, ";", SNOWFLAKE)) unexpected_token(t);
            Import* imp = malloc(sizeof(Import));
            imp->path = path;
            imp->wildcard = wildcard;
            imp->container = module;
            list_append(&module->imports, imp);
        } else {
            unexpected_token(t);
        }
    }
    return module;
}

Expression* parse_expresslet(TokenStream* stream, bool allow_lit) {
    Expression* expression = malloc(sizeof(Expression));
    Token* t = next_token(stream);
    CodePoint start = t->span.left;
    CodePoint end = t->span.right;
    
    if (token_compare(t, "(", SNOWFLAKE)) {
        expression = parse_expression(stream, true);
        t = next_token(stream);
        if (!token_compare(t, ")", SNOWFLAKE)) unexpected_token(t);
    } else if (token_compare(t, "let", IDENTIFIER)) {
        Identifier* name = parse_identifier(stream);
        end = name->span.right;
        LetExpr* let = malloc(sizeof(LetExpr));
        Variable* var = malloc(sizeof(Variable));
        var->path = path_simple(name);
        let->var = var;
        t = next_token(stream);
        if (token_compare(t, ":", SNOWFLAKE)) {
            let->type = parse_type_value(stream);
        } else {
            let->type = NULL;
            stream->peek = t;
        }
        t = next_token(stream);
        if (token_compare(t, "=", SNOWFLAKE)) {
            let->value = parse_expression(stream, allow_lit);
            end = let->value->span.right;
        } else {
            spanned_error("Let binding needs value", t->span, "let binding needs to be assigned with a value: let %s = ...;", name->name);
        }
        expression->expr = let;
        expression->type = EXPR_LET;
    } else if (token_compare(t, "return", IDENTIFIER)) {
        t = next_token(stream);
        stream->peek = t;
        if (token_compare(t, ";", SNOWFLAKE)) {
            expression->expr = NULL;
        } else {
            expression->expr = parse_expression(stream, allow_lit);
            end = ((Expression*)expression->expr)->span.right;
        }
        expression->type = EXPR_RETURN;
    } else if (token_compare(t, "break", IDENTIFIER)) {
        t = next_token(stream);
        stream->peek = t;
        if (token_compare(t, ";", SNOWFLAKE)) {
            expression->expr = NULL;
        } else {
            expression->expr = parse_expression(stream, allow_lit);
            end = ((Expression*)expression->expr)->span.right;
        }
        expression->type = EXPR_BREAK;
    } else if (token_compare(t, "continue", IDENTIFIER)) {
        expression->type = EXPR_CONTINUE;
        expression->expr = NULL;
    } else if (token_compare(t, "if", IDENTIFIER)) {
        Conditional* conditional = malloc(sizeof(Conditional));
        conditional->cond = parse_expression(stream, false);
        conditional->then = parse_block(stream);
        end = conditional->then->span.right;
        t = next_token(stream);
        if (token_compare(t, "else", IDENTIFIER)) {
            conditional->otherwise = parse_block(stream);
            end = conditional->otherwise->span.right;
        } else {
            conditional->otherwise = NULL;
            stream->peek = t;
        }
        expression->expr = conditional;
        expression->type = EXPR_CONDITIONAL;
    } else if (token_compare(t, "while", IDENTIFIER)) {
        WhileLoop* while_loop = malloc(sizeof(WhileLoop));
        while_loop->cond = parse_expression(stream, false);
        while_loop->body = parse_block(stream);
        end = while_loop->body->span.right;
        expression->expr = while_loop;
        expression->type = EXPR_WHILE_LOOP;
    } else if (t->type == IDENTIFIER || token_compare(t, ":", SNOWFLAKE)) {
        stream->peek = t;
        Path* path = parse_path(stream);
        if (path->elements.length > 0) end = path->elements.elements[path->elements.length - 1]->span.right;
        GenericValues* generics = NULL;
        t = next_token(stream);
        Token* gen_start = t;
        stream->peek = t;
        if (path->ends_in_double_colon && token_compare(t, "<", SNOWFLAKE)) {
            generics = parse_generic_values(stream);
        }
        t = next_token(stream);
        if (token_compare(t, "(", SNOWFLAKE)) {
            ExpressionList arguments = list_new(ExpressionList); 
            t = next_token(stream);
            if (!token_compare(t, ")", SNOWFLAKE)) {
                stream->peek = t;
                while (true) {
                    Expression* argument = parse_expression(stream, true);
                    list_append(&arguments, argument);
                    t = next_token(stream);
                    if (token_compare(t, ")", SNOWFLAKE)) {
                        end = t->span.right;
                        break;
                    } else if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
                }
            }
            FuncCall* call = malloc(sizeof(FuncCall));
            call->name = path;
            call->arguments = arguments;
            call->generics = generics;
            expression->expr = call;
            expression->type = EXPR_FUNC_CALL;       
        } else if (token_compare(t, "{", SNOWFLAKE) && allow_lit) {
            Map* fields = map_new();
            t = next_token(stream);
            if (!token_compare(t, "}", SNOWFLAKE)) {
                stream->peek = t;
                while (true) {
                    Identifier* field = parse_identifier(stream);
                    t = next_token(stream);
                    if (!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t);
                    Expression* value = parse_expression(stream, true);
                    StructFieldLit* sfl = malloc(sizeof(StructFieldLit));
                    sfl->name = field;
                    sfl->value = value;
                    map_put(fields, field->name, sfl);
                    t = next_token(stream);
                    if (token_compare(t, "}", SNOWFLAKE)) {
                        end = t->span.right;
                        break;
                    } else if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
                }
            }
            StructLiteral* slit = malloc(sizeof(StructLiteral));
            TypeValue* tv =malloc(sizeof(TypeValue));
            tv->name = path;
            tv->def = NULL;
            tv->generics = generics;
            tv->ctx = NULL;
            slit->type = tv;
            slit->fields = fields;
            expression->expr = slit;
            expression->type = EXPR_STRUCT_LITERAL;
        } else {
            if (generics != NULL) unexpected_token(gen_start);
            // other items are now allowed, disabling this check
            // if (path->absolute || path->elements.length != 1) panic("This path is not a single variable: %s @ %s", to_str_writer(stream, fprint_path(stream, path)), to_str_writer(stream, fprint_span(stream, &path->elements.elements[0]->span)));
            stream->peek = t;
            Variable* var = malloc(sizeof(Variable));
            var->path = path;
            expression->expr = var;
            expression->type = EXPR_VARIABLE;
        }
    } else if (t->type == NUMERAL || t->type == STRING) {
        expression->type = EXPR_LITERAL;
        expression->expr = t;
    } else unexpected_token(t);

    expression->span = from_points(&start, &end);
    while (true) {
        Token* t = next_token(stream);
        if (token_compare(t, ".", SNOWFLAKE)) {
            Identifier* field = parse_identifier(stream);
            FieldAccess* fa = malloc(sizeof(FieldAccess));
            fa->object = expression;
            fa->field = field;
            expression = malloc(sizeof(Expression));
            expression->span = from_points(&fa->object->span.left, &fa->field->span.right);
            expression->expr = fa;
            expression->type = EXPR_FIELD_ACCESS;
        } else {
            stream->peek = t;
            break;
        }
    }
    return expression;
}

int bin_op_precedence(str op) {
    if (str_eq("||", op)) return 0;
    if (str_eq("&&", op)) return 1;
    if (str_eq("==", op) || str_eq("!=", op) || str_eq(">", op) 
    || str_eq("<", op) || str_eq(">=", op) || str_eq("<=", op)) return 2;
    if (str_eq("|", op)) return 3;
    if (str_eq("^", op)) return 4;
    if (str_eq("&", op)) return 5;
    if (str_eq(">>", op) || str_eq("<<", op)) return 6;
    if (str_eq("+", op) || str_eq("-", op)) return 7;
    if (str_eq("*", op) || str_eq("/", op) || str_eq("%", op)) return 8;
    unreachable("Invalid binop %s", op);
}

Expression* parse_expression(TokenStream* stream, bool allow_lit) {
    Expression* expr = NULL;
    Expression** inner = &expr;

    while (true) {
        Token* t = next_token(stream);
        if (token_compare(t, "&", SNOWFLAKE)) {
            Expression* ref = malloc(sizeof(Expression));
            ref->span = t->span;
            ref->type = EXPR_TAKEREF;
            ref->expr = NULL;
            *inner = ref;
            inner = (Expression**)&ref->expr;
        } else if (token_compare(t, "*", SNOWFLAKE)) {
            Expression* deref = malloc(sizeof(Expression));
            deref->span = t->span;
            deref->type = EXPR_DEREF;
            deref->expr = NULL;
            *inner = deref;
            inner = (Expression**)&deref->expr;
        } else {
            stream->peek = t;
            *inner = parse_expresslet(stream, allow_lit);
            break;
        }
    }
    
    Token* t = next_token(stream);
    if (t->type == SNOWFLAKE) {
        if (str_eq("+", t->string) || str_eq("-", t->string) || str_eq("*", t->string) || str_eq("/", t->string)
            || str_eq("|", t->string) || str_eq("&", t->string) || str_eq("^", t->string) || str_eq("!", t->string)
            || str_eq("%", t->string) || str_eq(">", t->string) || str_eq("<", t->string) || str_eq("=", t->string)) {
            Token* n = next_token(stream);
            if (n->type == SNOWFLAKE && (str_eq("=", n->string) || str_eq("|", n->string) || str_eq("&", n->string) || str_eq("<", n->string) || str_eq(">", n->string))) {
                str s = t->string;
                t->string = malloc(3);
                t->string[0] = s[0];
                t->string[1] = n->string[0];
                t->string[2] = 0;
                t->span.right = n->span.right;
            } else {
                stream->peek = n;
                if (str_eq("=", t->string)) {
                    Expression* value = parse_expression(stream, allow_lit);
                    Assign* assign = malloc(sizeof(Assign));
                    assign->asignee = expr;
                    assign->value = value;
                    Expression* assignment = malloc(sizeof(Expression));
                    assignment->span = from_points(&expr->span.left, &value->span.right);
                    assignment->type = EXPR_ASSIGN;
                    assignment->expr = assign;
                    return assignment;
                }
            }
            Expression* rhs = parse_expression(stream, allow_lit);
            if (rhs->type == EXPR_BIN_OP) {
                BinOp* rhs_inner = rhs->expr;
                if (bin_op_precedence(t->string) >= bin_op_precedence(rhs_inner->op)) {
                    Expression* a = expr;
                    Expression* b = rhs_inner->lhs;
                    Expression* c = rhs_inner->rhs;
                    BinOp* op = malloc(sizeof(BinOp));
                    op->lhs = a;
                    op->rhs = b;
                    op->op = t->string;
                    op->op_span = t->span;
                    t->string = rhs_inner->op;
                    t->span = rhs_inner->op_span;
                    Expression* op_expr = malloc(sizeof(Expression));
                    op_expr->expr = op;
                    op_expr->type = EXPR_BIN_OP;
                    op_expr->span = from_points(&op->lhs->span.left, &op->rhs->span.right);
                    expr = op_expr;
                    rhs = c;
                }
            }
            BinOp* op = malloc(sizeof(BinOp));
            op->lhs = expr;
            op->rhs = rhs;
            op->op = t->string;
            op->op_span = t->span;
            Expression* parent = malloc(sizeof(Expression));
            parent->expr = op;
            parent->type = EXPR_BIN_OP;
            parent->span = from_points(&op->lhs->span.left, &op->rhs->span.right);
            expr = parent;
            return expr;
        } else {
            stream->peek = t;
            return expr;
        }
    } else {
        stream->peek = t;
        return expr;
    }
    unexpected_token(t, "unreachable: this is a compiler bug");
}

Path* parse_path(TokenStream* stream) {
    bool absolute = double_double_colon(stream);
    IdentList elements = list_new(IdentList);
    bool ends_in_double_colon = false;
    do {
        Token* t = next_token(stream);
        stream->peek = t;
        if (t->type != IDENTIFIER) {
            ends_in_double_colon = true;
            break;
        }
        list_append(&elements, parse_identifier(stream));
    } while (double_double_colon(stream));
    Path* path = path_new(absolute, elements);
    path->ends_in_double_colon = ends_in_double_colon;
    return path;
}

TypeValue* parse_type_value(TokenStream* stream) {
    Path* name = parse_path(stream);
    TypeValue* tval = malloc(sizeof(TypeValue));
    tval->name = name;
    tval->generics = NULL;
    tval->def = NULL;
    tval->ctx = NULL;
    Token* t = try_next_token(stream);
    if (t != NULL) {
        stream->peek = t;
        if (token_compare(t, "<", SNOWFLAKE)) {
            tval->generics = parse_generic_values(stream);
        }
    }
    return tval;
}

Identifier* parse_identifier(TokenStream* stream) {
    Token* t = next_token(stream);
    if (t->type != IDENTIFIER) unexpected_token(t);
    Identifier* identifier = malloc(sizeof(Identifier));
    identifier->name = t->string;
    identifier->span = t->span;;
    return identifier;
}

Block* parse_block(TokenStream* stream) {
    ExpressionList expressions = list_new(ExpressionList);
    bool yield_last = false;
    
    Token* t = next_token(stream);
    if (!token_compare(t, "{", SNOWFLAKE)) unexpected_token(t);
    CodePoint start = t->span.left;
    CodePoint end = t->span.right;
    t = next_token(stream);
    if (token_compare(t, "}", SNOWFLAKE)) goto empty;
    stream->peek = t;

    while (true) {
        Expression* expression = parse_expression(stream, true);
        list_append(&expressions, expression);
        t = next_token(stream);
        if (!token_compare(t, ";", SNOWFLAKE)) {
            if (token_compare(t, "}", SNOWFLAKE)) {
                yield_last = true;
                end = t->span.right;
                break;
            } else {
                unexpected_token(t, "Maybe missing semicolon?");
            }
        }
        t = next_token(stream);
        if (token_compare(t, "}", SNOWFLAKE)) {
            end = t->span.right;
            break;
        }
        stream->peek = t;
    }
empty: {}
    Block* block = malloc(sizeof(Block));
    block->expressions = expressions;
    block->yield_last = yield_last;
    block->span = from_points(&start, &end);
    return block;
}

GenericKeys* parse_generic_keys(TokenStream* stream) {
    Token* t = next_token(stream);
    CodePoint left = t->span.left;
    IdentList generics = list_new(IdentList);
    if (!token_compare(t, "<", SNOWFLAKE)) unexpected_token(t);
    t = next_token(stream);
    if (!token_compare(t, ">", SNOWFLAKE)) {
        stream->peek = t;
        while (true) {    
            Identifier* generic = parse_identifier(stream);
            list_append(&generics, generic);
            t = next_token(stream);
            if (token_compare(t, ">", SNOWFLAKE)) break;
            if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
        }
    }
    CodePoint right = t->span.right;
    GenericKeys* keys = malloc(sizeof(GenericKeys));
    keys->generics = generics;
    keys->span = from_points(&left, &right);
    keys->resolved = map_new();
    keys->generic_uses = map_new();
    keys->generic_use_keys = list_new(StrList);
    return keys;
}

GenericValues* parse_generic_values(TokenStream* stream) {
    Token* t = next_token(stream);
    GenericValues* generics = malloc(sizeof(GenericValues));
    generics->span.left = t->span.left;
    generics->generics = list_new(TypeValueList);
    generics->resolved = map_new();
    t = next_token(stream);
    if (!token_compare(t, ">", SNOWFLAKE)) {
        stream->peek = t;
        while (true) {
            TypeValue* g = parse_type_value(stream);
            list_append(&generics->generics, g);
            t = next_token(stream);
            if (token_compare(t, ">", SNOWFLAKE)) break;
            if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
        }
    }
    generics->span.right = t->span.right;
    return generics;
}

FuncDef* parse_function_definition(TokenStream* stream) {
    Token* t = next_token(stream);
    if (!token_compare(t, "fn", IDENTIFIER)) unexpected_token(t);
    Identifier* name = parse_identifier(stream);
    GenericKeys* keys = NULL;
    t = next_token(stream);
    if (token_compare(t, "<", SNOWFLAKE)) {
        stream->peek = t;
        keys = parse_generic_keys(stream);
        t = next_token(stream);
    }
    if (!token_compare(t, "(", SNOWFLAKE)) unexpected_token(t);
    t = next_token(stream);
    ArgumentList arguments = list_new(ArgumentList); 
    bool variadic = false;
    if (!token_compare(t, ")", SNOWFLAKE)) {
        stream->peek = t;
        while (true) {
            t = next_token(stream);
            if (token_compare(t, "*", SNOWFLAKE)) {
                variadic = true;
                t = next_token(stream);
                if (!token_compare(t, ")", SNOWFLAKE)) unexpected_token(t);
                break;
            } else {
                stream->peek = t;
            }
            Argument* argument = malloc(sizeof(Argument));
            Variable* var = malloc(sizeof(Variable));
            var->path = path_simple(parse_identifier(stream));
            argument->var = var;
            t = next_token(stream);
            if (!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t);
            argument->type = parse_type_value(stream);
            list_append(&arguments, argument);
            t = next_token(stream);
            if (token_compare(t, ")", SNOWFLAKE)) {
                break;
            } else if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
        }
    }
    TypeValue* return_type = NULL;
    t = next_token(stream);
    Block* body = NULL;
    if (token_compare(t, ";", SNOWFLAKE)) goto no_body;
    if (!token_compare(t, "{", SNOWFLAKE)) {
        if (!token_compare(t, "-", SNOWFLAKE)) unexpected_token(t);
        t = next_token(stream);
        if (!token_compare(t, ">", SNOWFLAKE)) unexpected_token(t);
        return_type = parse_type_value(stream);
        t = next_token(stream);
        if (token_compare(t, ";", SNOWFLAKE)) goto no_body;
        stream->peek = t;
    } else {
        stream->peek = t;
    }
    body = parse_block(stream);
    no_body: {}
    FuncDef* func = malloc(sizeof(FuncDef));
    func->body = body;
    func->name = name;
    func->return_type = return_type;
    func->args = arguments;
    func->is_variadic = variadic;
    func->generics = keys;
    func->head_resolved = false;
    return func;
}