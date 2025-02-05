#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "parser.h"
#include "ast.h"
#include "lib.h"
#include "lib/exit.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
LIB;
#include "module.h"
#include "token.h"

bool double_double_colon(TokenStream* stream) {
    Token* t1 = try_next_token(stream);
    if (t1 == NULL) return false;
    if (!token_compare(t1, ":", SNOWFLAKE)) {
        stream->peek = t1;
        return false;
    }
    Token* t2 = next_token(stream);
    if (!token_compare(t2, ":", SNOWFLAKE)) {
        stream->ppeek = t1;
        stream->peek = t2;
        return false;
    }
    return true;
}

AnnoList parse_annotations(TokenStream* stream) {
    AnnoList list = list_new(AnnoList);
    Token* t = next_token(stream);
    while (token_compare(t, "#", SNOWFLAKE)) {
        t = next_token(stream);
        if (!token_compare(t, "[", SNOWFLAKE)) unexpected_token(t, "expected #[...]");
        Path* path = parse_path(stream);
        Annotation annotation;
        annotation.path = path;
        annotation.data = NULL;
        annotation.type = AT_FLAG;
        t = next_token(stream);
        if (token_compare(t, "=", SNOWFLAKE)) {
            t = next_token(stream);
            if (t->type != IDENTIFIER && t->type != NUMERAL && t->type != STRING) unexpected_token(t, "expected #[%s=?] where ? is IDENTIFIER, NUMERAL or STRING", to_str_writer(s, fprint_path(s, path)));
            annotation.data = t;
            annotation.type = AT_DEFINE;
            t = next_token(stream);
        }
        list_append(&list, annotation);
        if (!token_compare(t, "]", SNOWFLAKE)) unexpected_token(t, "expected #[...]");
        t = next_token(stream);
    }
    stream->peek = t;
    return list;
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
    type->traits = list_new(TraitList);
    return type;
}


ImplBlock* parse_impl(TokenStream* stream, Module* module) {
    ImplBlock* impl = malloc(sizeof(ImplBlock));
    impl->methods = map_new();
    Token* t = next_token(stream);
    if (!token_compare(t, "impl", IDENTIFIER)) unexpected_token(t);
    t = peek_next_token(stream);
    impl->generics = NULL;
    if (token_compare(t, "<", SNOWFLAKE)) {
        impl->generics = parse_generic_keys(stream);
    }
    impl->type = parse_type_value(stream);
    t = next_token(stream);
    impl->trait = NULL;
    if (token_compare(t, ":", SNOWFLAKE)) {
        impl->trait_ref = parse_type_value(stream);
        if (impl->trait_ref->name->elements.length == 0) panic("");
        t = next_token(stream);
    } else {
        impl->trait_ref = NULL;
    }
    if (!token_compare(t, "{", SNOWFLAKE)) unexpected_token(t);
    while (true) {
        t = next_token(stream);
        if (token_compare(t, "}", SNOWFLAKE)) break;
        stream->peek = t;
        AnnoList annos = parse_annotations(stream);

        Visibility vis = V_PRIVATE;
        if (token_compare(t, "pub", IDENTIFIER)) {
            vis = V_PUBLIC;
            t = next_token(stream); // skip peek 
            t = next_token(stream); // setting new peek
            stream->peek = t;
        } else if (token_compare(t, "loc", IDENTIFIER)) {
            vis = V_LOCAL;
            t = next_token(stream); // skip peek 
            t = next_token(stream); // setting new peek
            stream->peek = t;
        }
        FuncDef* func = parse_function_definition(stream);
        func->module = module;
        func->annotations = annos;
        func->impl_type = impl->type;
        func->impl_block = impl;
        if (func->generics == NULL) {
            func->generics = malloc(sizeof(GenericKeys));
            func->generics->generic_use_keys = list_new(StrList);
            func->generics->generic_uses = map_new();
            func->generics->resolved = map_new();
            func->generics->generics = list_new(GKeyList);
            func->generics->span = func->name->span;
        }
        if (impl->generics != NULL) {
            func->type_generics = malloc(sizeof(GenericKeys));
            func->type_generics->span = impl->generics->span;
            func->type_generics->generic_use_keys = list_new(StrList);
            func->type_generics->generic_uses = map_new();
            func->type_generics->resolved = map_new();
            func->type_generics->generics = list_new(GKeyList);
            list_foreach(&impl->generics->generics, i, GKey* key, { list_append(&func->type_generics->generics, key); });
        }
        ModuleItem* mi = malloc(sizeof(ModuleItem));
        mi->item = func;
        mi->type = MIT_FUNCTION;
        mi->module = module;
        mi->origin = NULL;
        mi->vis = vis;
        mi->name = func->name;
        ModuleItem* old = map_put(impl->methods, func->name->name, mi);
        if (old != NULL) spanned_error("Methiod already exists", mi->name->span, "Method `%s` already defined in this impl block @ %s", old->name->name, to_str_writer(s, fprint_span(s, &old->name->span)));
    }
    return impl;
}

TraitDef* parse_traitdef(TokenStream* stream, Module* module) {
    TraitDef* trait = malloc(sizeof(TraitDef));
    trait->head_resolved = false;
    trait->methods = map_new();
    trait->module = module;
    Token* t = next_token(stream);
    if (!token_compare(t, "trait", IDENTIFIER)) unexpected_token(t);
    trait->self_key = malloc(sizeof(GKey));
    trait->self_key->name = parse_identifier(stream);
    t = next_token(stream);
    if (!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t);
    trait->name = parse_identifier(stream);
    trait->self_key->bounds = list_new(TraitBoundList);
    TypeValue* selfbound = malloc(sizeof(TypeValue));
    selfbound->ctx = NULL;
    selfbound->generics = NULL;
    selfbound->trait_impls = map_new();
    selfbound->name = path_new(true, module->path->elements);
    list_append(&selfbound->name->elements, trait->name);
    TraitBound* bound = malloc(sizeof(TraitBound));
    bound->bound = selfbound;
    bound->resolved = NULL;
    list_append(&trait->self_key->bounds, bound);
    t = peek_next_token(stream);
    trait->keys = NULL;
    if (token_compare(t, "<", SNOWFLAKE)) {
        trait->keys = parse_generic_keys(stream);
    } else {
        trait->keys = malloc(sizeof(GenericKeys));
        trait->keys->generic_use_keys = list_new(StrList);
        trait->keys->generic_uses = map_new();
        trait->keys->resolved = map_new();
        trait->keys->generics = list_new(GKeyList);
        trait->keys->span = trait->name->span;
    }

    trait->self_type = malloc(sizeof(TypeDef));
    trait->self_type->annotations = list_new(AnnoList);
    trait->self_type->flist = list_new(IdentList);
    trait->self_type->name = trait->self_key->name;
    trait->self_type->extern_ref = NULL;
    trait->self_type->fields = map_new();
    trait->self_type->transpile_state = 0;
    trait->self_type->generics = NULL;
    trait->self_type->module = NULL;
    trait->self_type->head_resolved = true;
    trait->self_type->traits = list_new(TraitList);
    list_append(&trait->self_type->traits, trait);

    trait->self_key->bounds.elements[0]->bound->def = trait->self_type;

    trait->self_tv = malloc(sizeof(TypeValue));
    trait->self_tv->ctx = NULL;
    trait->self_tv->def = trait->self_type;
    trait->self_tv->generics = NULL;
    trait->self_tv->name = path_simple(trait->self_key->name);
    trait->self_tv->trait_impls = map_new();
    map_put(trait->self_tv->trait_impls, to_str_writer(s, fprintf(s, "%p", trait)), trait);

    t = next_token(stream);
    if (!token_compare(t, "{", SNOWFLAKE)) unexpected_token(t);
    while (true) {
        t = next_token(stream);
        if (token_compare(t, "}", SNOWFLAKE)) break;
        stream->peek = t;
        AnnoList annos = parse_annotations(stream);
        if (annos.length > 0) spanned_error("Invalid annotations", t->span, "Function annotations in trait definitions not supported");
        FuncDef* func = parse_function_definition(stream);
        if (func->body != NULL) spanned_error("Method body inside trait definition", func->name->span, "Expected method definition `fn %s(...);`, got method with body.", func->name->name);
        func->module = module;
        func->annotations = annos;
        func->impl_type = trait->self_tv;
        func->trait_def = true;
        func->trait = trait;
        func->impl_block = NULL;
        if (func->generics == NULL) {
            func->generics = malloc(sizeof(GenericKeys));
            func->generics->generic_use_keys = list_new(StrList);
            func->generics->generic_uses = map_new();
            func->generics->resolved = map_new();
            func->generics->generics = list_new(GKeyList);
            func->generics->span = func->name->span;
        }
        /*func->type_generics = malloc(sizeof(GenericKeys));
        func->type_generics->span = trait->keys->span;
        func->type_generics->generic_use_keys = list_new(StrList);
        func->type_generics->generic_uses = map_new();
        func->type_generics->resolved = map_new();
        func->type_generics->generics = list_new(GKeyList);
        list_foreach(&trait->keys->generics, i, GKey* key, { list_append(&func->type_generics->generics, key); });*/
        func->type_generics = trait->keys; // undoing the above 7 lines
        ModuleItem* mi = malloc(sizeof(ModuleItem));
        mi->item = func;
        mi->type = MIT_FUNCTION;
        mi->module = module;
        mi->origin = NULL;
        mi->vis = V_PUBLIC;
        mi->name = func->name;
        ModuleItem* old = map_put(trait->methods, func->name->name, mi);
        if (old != NULL) spanned_error("Methiod already exists", mi->name->span, "Method `%s` already defined in this impl block @ %s", old->name->name, to_str_writer(s, fprint_span(s, &old->name->span)));
    }
    return trait;
}


Module* parse_module_contents(TokenStream* stream, Path* path) {
    Module* module = malloc(sizeof(Module));
    module->imports = list_new(ImportList);
    module->impls = list_new(ImplList);
    module->items = map_new();
    module->path = path;
    module->resolved = false;
    module->in_resolution = false;
    module->filepath = NULL;
    module->name = path->elements.elements[path->elements.length-1];
    module->subs = list_new(ModDefList);

    while (has_next(stream)) {
        AnnoList annos = parse_annotations(stream);

        Token* t = peek_next_token(stream);

        Visibility vis = V_PRIVATE;
        if (token_compare(t, "pub", IDENTIFIER)) {
            vis = V_PUBLIC;
            t = next_token(stream); // skip peek 
            t = next_token(stream); // setting new peek
            stream->peek = t;
        } else if (token_compare(t, "loc", IDENTIFIER)) {
            vis = V_LOCAL;
            t = next_token(stream); // skip peek 
            t = next_token(stream); // setting new peek
            stream->peek = t;
        }
        if (token_compare(t, "pun", IDENTIFIER) || token_compare(t, "pn", IDENTIFIER)) {
            spanned_error("Invalid declaration", t->span, "punction cannot be declared here");
        } else if (token_compare(t, "fn", IDENTIFIER)) {
            FuncDef* function = parse_function_definition(stream);
            function->annotations = annos;
            function->module = module;
            ModuleItem* mi = malloc(sizeof(ModuleItem));
            mi->item = function;
            mi->type = MIT_FUNCTION;
            mi->module = module;
            mi->origin = NULL;
            mi->vis = vis;
            mi->name = function->name;
            ModuleItem* old = map_put(module->items, function->name->name, mi);
            if (old != NULL) {
                spanned_error("Name conflict", function->name->span, "Name %s is already defined in this scope at %s", function->name->name, to_str_writer(s, fprint_span(s, &old->name->span)));
            }
        } else if (token_compare(t, "struct", IDENTIFIER)) {
            TypeDef* type = parse_struct(stream);
            type->annotations = annos;
            type->module = module;
            ModuleItem* mi = malloc(sizeof(ModuleItem));
            mi->item = type;
            mi->type = MIT_STRUCT;
            mi->module = module;
            mi->origin = NULL;
            mi->vis = vis;
            mi->name = type->name;
            ModuleItem* old = map_put(module->items, type->name->name, mi);
            if (old != NULL) {
                spanned_error("Name conflict", type->name->span, "Name %s is already defined in this scope at %s", type->name->name, to_str_writer(s, fprint_span(s, &old->name->span)));
            }
        } else if (token_compare(t, "mod", IDENTIFIER)) {
            t = next_token(stream); // skip mod
            Identifier* name = parse_identifier(stream);
            if (annos.length > 0) spanned_error("Annotated module definition", name->span, "Module definitions dont take annotations yet.");
            t = next_token(stream);
            if (!token_compare(t, ";", SNOWFLAKE)) unexpected_token(t);
            ModDef* mod = malloc(sizeof(ModDef));
            mod->name = name;
            mod->vis = vis;
            list_append(&module->subs, mod);
        } else if (token_compare(t, "use", IDENTIFIER)) {
            t = next_token(stream); // skip use
            Path* path = parse_path(stream);
            if (annos.length > 0) spanned_error("Annotated use", path->elements.elements[0]->span, "Item import does not take annotations yet.");
            bool wildcard = false;
            Identifier* alias = NULL;
            if (path->ends_in_double_colon) {
                t = next_token(stream);
                if (token_compare(t, "*", SNOWFLAKE)) wildcard = true;
                else stream->peek = t;
            } else {
                t = next_token(stream);
                if (token_compare(t, "as", IDENTIFIER)) {
                    alias = parse_identifier(stream);
                } else {
                    stream->peek = t;
                }
            }
            t = next_token(stream);
            if (!token_compare(t, ";", SNOWFLAKE)) unexpected_token(t);
            Import* imp = malloc(sizeof(Import));
            imp->path = path;
            imp->wildcard = wildcard;
            imp->container = module;
            imp->vis = vis;
            imp->alias = alias;
            list_append(&module->imports, imp);
        } else if (token_compare(t, "impl", IDENTIFIER)) {
            ImplBlock* impl = parse_impl(stream, module);
            list_append(&module->impls, impl);
        } else if (token_compare(t, "trait", IDENTIFIER)) {
            TraitDef* trait = parse_traitdef(stream, module);
            ModuleItem* mi = malloc(sizeof(ModuleItem));
            mi->item = trait;
            mi->type = MIT_TRAIT;
            mi->module = module;
            mi->origin = NULL;
            mi->vis = vis;
            mi->name = trait->name;
            ModuleItem* old = map_put(module->items, trait->name->name, mi);
            if (old != NULL) {
                spanned_error("Name conflict", trait->name->span, "Name %s is already defined in this scope at %s", trait->name->name, to_str_writer(s, fprint_span(s, &old->name->span)));
            }
        } else if(token_compare(t, "static", IDENTIFIER) || token_compare(t, "const", IDENTIFIER)) {
            bool constant = token_compare(t, "const", IDENTIFIER);
            t = next_token(stream); // skip static
            Global* g = malloc(sizeof(Global));
            g->name = parse_identifier(stream);
            t = next_token(stream);
            if(!token_compare(t, ":", SNOWFLAKE)) unexpected_token(t, "static|const %s: ...;", g->name->name);
            g->type = parse_type_value(stream);
            t = next_token(stream);
            g->module = module;
            g->annotations = annos;
            g->constant = constant;
            g->computed_value = NULL;
            g->value = NULL;
            if(!token_compare(t, ";", SNOWFLAKE)) {
                if(!token_compare(t, "=", SNOWFLAKE)) unexpected_token(t, "static|const %s: ... = ...;", g->name->name);
                g->value = parse_expression(stream, true);
                t = next_token(stream);
                if(!token_compare(t, ";", SNOWFLAKE)) unexpected_token(t, "static|const %s: ... = ...;", g->name->name);
            }
            ModuleItem* mi = malloc(sizeof(ModuleItem));
            mi->item = g;
            if (constant) mi->type = MIT_CONSTANT;
            else mi->type = MIT_STATIC;
            mi->module = module;
            mi->origin = NULL;
            mi->vis = vis;
            mi->name = g->name;
            ModuleItem* old = map_put(module->items, mi->name->name, mi);
            if (old != NULL) {
                spanned_error("Name conflict", mi->name->span, "Name %s is already defined in this scope at %s", mi->name->name, to_str_writer(s, fprint_span(s, &old->name->span)));
            }
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
    
    if (token_compare(t, "{", SNOWFLAKE)) {
        stream->peek = t;
        expression->expr = parse_block(stream);
        expression->type = EXPR_BLOCK;
    } else if (token_compare(t, "(", SNOWFLAKE)) {
        expression = parse_expression(stream, true);
        t = next_token(stream);
        if (!token_compare(t, ")", SNOWFLAKE)) unexpected_token(t);
        t = next_token(stream);
        if (token_compare(t, "(", SNOWFLAKE)) {
            DynRawCall* call = malloc(sizeof(DynRawCall));
            call->callee = expression;
            call->args = list_new(ExpressionList);
            t = peek_next_token(stream);
            if (!token_compare(t, ")", SNOWFLAKE)) {
                while (true) {
                    Expression* argument = parse_expression(stream, true);
                    list_append(&call->args, argument);
                    t = next_token(stream);
                    if (token_compare(t, ")", SNOWFLAKE)) {
                        end = t->span.right;
                        break;
                    } else if (!token_compare(t, ",", SNOWFLAKE)) unexpected_token(t);
                }
            } else {
                t = next_token(stream);
            }
            expression = malloc(sizeof(Expression));
            expression->expr = call;
            expression->type = EXPR_DYN_RAW_CALL;
        } else {
            stream->peek = t;
        }
    } else if (token_compare(t, "let", IDENTIFIER)) {
        Identifier* name = parse_identifier(stream);
        end = name->span.right;
        LetExpr* let = malloc(sizeof(LetExpr));
        Variable* var = malloc(sizeof(Variable));
        var->global_ref = NULL;
        var->path = path_simple(name);
        var->values= NULL;
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
        t = peek_next_token(stream);
        if (token_compare(t, ";", SNOWFLAKE)) {
            expression->expr = NULL;
        } else {
            expression->expr = parse_expression(stream, allow_lit);
            end = ((Expression*)expression->expr)->span.right;
        }
        expression->type = EXPR_RETURN;
    } else if (token_compare(t, "break", IDENTIFIER)) {
        t = peek_next_token(stream);
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
            t = peek_next_token(stream);
            if (token_compare(t, "if", IDENTIFIER)) {
                Expression* otherwise = parse_expresslet(stream, true);
                if (otherwise->type != EXPR_CONDITIONAL) spanned_error("Invalid continuation of else", otherwise->span, "Expected `else if ... { ... }`, got %s", ExprType__NAMES[otherwise->type]);
                conditional->otherwise = malloc(sizeof(Block));
                conditional->otherwise->span = otherwise->span;
                conditional->otherwise->expressions = list_new(ExpressionList);
                list_append(&conditional->otherwise->expressions, otherwise);
                conditional->otherwise->yield_last = true;
            } else {
                conditional->otherwise = parse_block(stream);
            }
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
        t = peek_next_token(stream);
        if (path->ends_in_double_colon && token_compare(t, "<", SNOWFLAKE)) {
            generics = parse_generic_values(stream);
        }
        t = peek_next_token(stream);
        if (double_double_colon(stream)) {
            if (generics == NULL) unexpected_token(t);
            Identifier* method = parse_identifier(stream);
            GenericValues* method_generics = NULL;
            if (double_double_colon(stream)) {
                method_generics = parse_generic_values(stream);
            }
            t = peek_next_token(stream);
            if (token_compare(t, "(", SNOWFLAKE)) {
                t = next_token(stream);
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
                StaticMethodCall* call = malloc(sizeof(StaticMethodCall));
                call->arguments = arguments;
                call->name = method;
                call->generics = method_generics;
                call->tv = malloc(sizeof(TypeValue));
                call->tv->generics = generics;
                call->tv->name = path;
                call->tv->def = NULL;
                call->tv->ctx = NULL;
                call->tv->trait_impls = map_new();
                expression->expr = call;
                expression->type = EXPR_STATIC_METHOD_CALL;
            } else {
                Variable* var = malloc(sizeof(Variable));
                var->path = path;
                var->values = generics;
                var->method_name = method;
                var->method_values = method_generics;
                var->global_ref = NULL;
                expression->expr = var;
                expression->type = EXPR_VARIABLE;
            }
        } else if (token_compare(t, "(", SNOWFLAKE)) {
            t = next_token(stream);
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
            t = next_token(stream);
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
            tv->generics = generics;
            tv->def = NULL;
            tv->ctx = NULL;
            tv->trait_impls = map_new();
            slit->type = tv;
            slit->fields = fields;
            expression->expr = slit;
            expression->type = EXPR_STRUCT_LITERAL;
        } else {
            // other items are now allowed, disabling this check
            // if (path->absolute || path->elements.length != 1) panic("This path is not a single variable: %s @ %s", to_str_writer(stream, fprint_path(stream, path)), to_str_writer(stream, fprint_span(stream, &path->elements.elements[0]->span)));
            stream->peek = t;
            Variable* var = malloc(sizeof(Variable));
            var->path = path;
            var->values = generics;
            var->method_name = NULL;
            var->method_values = NULL;
            var->global_ref = NULL;
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
            bool will_call = false;
            GenericValues* generics = NULL;
            if (double_double_colon(stream)) {
                will_call = true;
                t = peek_next_token(stream);
                if (token_compare(t, "<", SNOWFLAKE)) {
                    generics = parse_generic_values(stream);
                }
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
                MethodCall* call = malloc(sizeof(MethodCall));
                call->object = expression;
                call->name = field;
                call->arguments = arguments;
                call->generics = generics;
                call->def = NULL;
                expression = malloc(sizeof(Expression));
                expression->span = from_points(&call->object->span.left, &call->name->span.right);
                expression->expr = call;
                expression->type = EXPR_METHOD_CALL;
            } else {
                if (will_call) spanned_error("Expected method call", t->span, "Expected method call parenthesis after `::`");
                stream->peek = t;
                FieldAccess* fa = malloc(sizeof(FieldAccess));
                fa->object = expression;
                fa->field = field;
                fa->is_ref = false;
                expression = malloc(sizeof(Expression));
                expression->span = from_points(&fa->object->span.left, &fa->field->span.right);
                expression->expr = fa;
                expression->type = EXPR_FIELD_ACCESS;
            }
        } else {
            stream->peek = t;
            break;
        }
    }
    return expression;
}

int bin_op_precedence(str op, Span span) {
    if (str_eq("||", op)) return 0;
    if (str_eq("&&", op)) return 1;
    if (str_eq("==", op) || str_eq("!=", op)
      || str_eq(">", op) || str_eq("<", op)
     || str_eq(">=", op) || str_eq("<=", op)) return 2;
    if (str_eq("|", op)) return 3;
    if (str_eq("^", op)) return 4;
    if (str_eq("&", op)) return 5;
    if (str_eq(">>", op) || str_eq("<<", op)) return 6;
    if (str_eq("+", op) || str_eq("-", op)) return 7;
    if (str_eq("*", op) || str_eq("/", op) || str_eq("%", op)) return 8;
    spanned_error("Invalid operator", span, "Invalid binop %s", op);
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
            if (str_endswith(t->string, "=") && t->string[0] != '=' && t->string[0] != '>' && t->string[0] != '<' && t->string[0] != '!') { // sth like `+=`
                usize l = strlen(t->string);
                t->string[l-1] = '\0';
                bin_op_precedence(t->string, t->span); // make sure op is valid
                BinOp* op = malloc(sizeof(BinOp));
                op->lhs = expr;
                op->rhs = rhs;
                op->op = t->string;
                op->op_span = t->span;
                Expression* parent = malloc(sizeof(Expression));
                parent->expr = op;
                parent->type = EXPR_BIN_OP_ASSIGN;
                parent->span = from_points(&op->lhs->span.left, &op->rhs->span.right);
                expr = parent;
                return expr;
            } else {
                bin_op_precedence(t->string, t->span); // make sure op is valid
                if (rhs->type == EXPR_BIN_OP) {
                    BinOp* rhs_inner = rhs->expr;
                    if (bin_op_precedence(t->string, t->span) > bin_op_precedence(rhs_inner->op, rhs_inner->op_span)) {
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
            }
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
        Token* t = peek_next_token(stream);
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
    tval->trait_impls = map_new();
    Token* t = peek_next_token(stream);
    if (t != NULL) {
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
    GKeyList generics = list_new(GKeyList);
    if (!token_compare(t, "<", SNOWFLAKE)) unexpected_token(t);
    t = next_token(stream);
    if (!token_compare(t, ">", SNOWFLAKE)) {
        stream->peek = t;
        while (true) {    
            Identifier* generic = parse_identifier(stream);
            GKey* gkey = malloc(sizeof(GKey));
            gkey->name = generic;
            gkey->bounds = list_new(TraitBoundList);
            t = next_token(stream);
            if (token_compare(t, ":", SNOWFLAKE)) {
                while (true) {
                    TypeValue* bound = parse_type_value(stream);
                    TraitBound* tb = malloc(sizeof(TraitBound));
                    tb->bound = bound;
                    tb->resolved = NULL;
                    list_append(&gkey->bounds, tb);
                    t = next_token(stream);
                    if (!token_compare(t, "+", SNOWFLAKE)) {
                        break;
                    }
                }
            }
            list_append(&generics, gkey);
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
            var->global_ref = NULL;
            var->values = NULL;
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
    func->impl_type = NULL;
    func->trait = NULL;
    func->annotations = list_new(AnnoList);
    func->type_generics = NULL;
    func->untraced = false;
    func->trait_def = false;
    return func;
}