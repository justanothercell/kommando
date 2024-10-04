#ifndef AST_H
#define AST_H

#include "lib.h"
#include "lib/enum.h"
#include "lib/list.h"
#include "token.h"

typedef struct Identifier {
    str name;
    Span span;
} Identifier;
LIST(IdentList, Identifier*);
typedef struct Path {
    IdentList elements;
    bool absolute;
} Path;
LIST(PathList, Path*);
Path* path_new(bool absolute, IdentList elements);
void path_append(Path* parent, Identifier* child);
Path* path_join(Path* parent, Path* child);
void fprint_path(FILE* file, Path* path);

typedef struct TypeDef {
    Identifier* name;
    IdentList generics;
} TypeDef;
typedef struct TypeValue TypeValue;
LIST(TypeValueList, TypeValue*);
typedef struct TypeValue {
    Path* name;
    TypeValueList generics;
    TypeDef* def;
} TypeValue;
void fprint_typevalue(FILE* file, TypeValue* tval);

typedef struct Variable {
    Identifier* name;
    usize id;
} Variable;

ENUM(ExprType, 
    EXPR_BIN_OP,
    EXPR_FUNC_CALL,
    EXPR_LITERAL,  // typeof(expr) = Token*
    EXPR_BLOCK,
    EXPR_VARIABLE,
    EXPR_LET,
    EXPR_CONDITIONAL,
    EXPR_WHILE_LOOP,
    EXPR_RETURN,   // typeof(expr) = Expression*
    EXPR_BREAK,    // typeof(expr) = Expression*
    EXPR_CONTINUE, // no data
);

typedef struct TVBox {
    TypeValue* type;
} TVBox;

typedef struct Expression {
    ExprType type;
    void* expr;
    Span span;
    TVBox* resolved;
} Expression;
void fprint_expression(FILE* file, Expression* expression);
LIST(ExpressionList, Expression*);

typedef struct BinOp {
    Expression* lhs;
    Expression* rhs;
    str op;
    Span op_span;
} BinOp;

typedef struct FuncCall {
    Path* name;
    ExpressionList arguments;
} FuncCall;

typedef struct Block {
    ExpressionList expressions;
    bool yield_last;
    Span span;
} Block;

typedef struct LetExpr {
    Variable* var;
    TypeValue* type;
    Expression* value;
} LetExpr;

typedef struct Conditional {
    Expression* cond;
    Block* then;
    Block* otherwise;
} Conditional;

typedef struct WhileLoop {
    Expression* cond;
    Block* body;
} WhileLoop;

typedef struct Argument {
    Variable* var;
    TypeValue* type;
} Argument;
LIST(ArgumentList, Argument*);
typedef struct FuncDef {
    Identifier* name;
    Block* body;
    TypeValue* return_type;
    ArgumentList args;
    bool is_variadic;
} FuncDef;

#endif