#ifndef AST_H
#define AST_H

#include "tokens.h"
#include "types.h"

typedef enum ExprType {
    FUNC_CALL_EXPR,
    BLOCK_EXPR,
    IF_EXPR,
    WHILE_EXPR,
    LITERAL_EXPR,
    VARIABLE_EXPR,
    RETURN_EXPR,
    STRUCT_LITERAL,
    REF_EXPR,
    DEREF_EXPR,
    VAR_DECL_EXPR,
    ASSIGN_EXPR,
    FIELD_ACCESS_EXPR,
    BINOP_EXPR
} ExprType;

typedef struct Expression {
    ExprType kind;
    any expr;
    str type;
    usize src_line;
} Expression;

LIST(ExpressionList, Expression*);

typedef struct Block {
    ExpressionList exprs;
} Block;

typedef struct Assign {
    Expression* target;
    Expression* value;
} Assign;

typedef struct BinOp {
    Expression* lhs;
    Expression* rhs;
    str op;
    usize precedence;
} BinOp;


typedef struct FieldAccess {
    Expression* object;
    Expression* field;
} FieldAccess;

typedef struct IfExpr {
    Expression* condition;
    Block* then;
    Block* otherwise;
} IfExpr;

typedef struct VarDecl {
    Expression* value;
    str type;
    str name;
} VarDecl;

typedef struct WhileExpr {
    Expression* condition;
    Block* body;
} WhileExpr;

typedef struct FunctionCall {
    str name;
    ExpressionList args;
} FunctionCall;

typedef struct FunctionDef {
    str name;
    Block* body;
    str ret_t;
    StrList args_t;
    StrList args;
    bool is_variadic;
    usize src_line;
} FunctionDef;
LIST(FunctionDefList, FunctionDef*);

void drop_expression(Expression* expression);

void drop_var_decl(VarDecl* vd);

void drop_assign(Assign* ass);

void drop_binop(BinOp* op);

void drop_field_access(FieldAccess* fa);

void drop_block(Block* block);

void drop_if_expr(IfExpr* if_expr);

void drop_while_expr(WhileExpr* while_expr);

void drop_function_call(FunctionCall* call);

void drop_expression(Expression* expr);

void drop_function_def(FunctionDef* func);

#define parse_block(stream) TRACE(__parse_block(stream))
Block* __parse_block(TokenStream* stream);

#define parse_type_value(stream) TRACE(__parse_type_value(stream))
TypeValue* __parse_type_value(TokenStream* stream);

#define parse_expression(stream) TRACE(__parse_expression(stream))
Expression* __parse_expression(TokenStream* stream);

#define parse_subscript(stream) TRACE(__parse_subscript(stream))
Expression* __parse_subscript(TokenStream* stream);

#define parse_expresslet(stream) TRACE(__parse_expresslet(stream))
Expression* __parse_expresslet(TokenStream* stream);

#define parse_type_def(stream) TRACE(__parse_type_def(stream))
TypeDef* __parse_type_def(TokenStream* stream);

#define parse_function_def(stream) TRACE(__parse_function_def(stream))
FunctionDef* __parse_function_def(TokenStream* stream);

typedef struct Module {
    FunctionDefList funcs;
    TypeDefList types;
} Module;

void drop_module(Module* module);

static TypeList TEMP_TYPES;
void drop_temp_types();
Type* find_type(Module* module, str type, usize src_line);

FunctionDef* find_func(Module* module, str func, usize src_line);

#define parse_module(stream)  TRACE(_parse_module(stream))
Module* _parse_module(TokenStream* stream);

#endif