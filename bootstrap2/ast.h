#ifndef AST_H
#define AST_H

#include "lib.h"
#include "lib/enum.h"
#include "lib/list.h"
#include "lib/map.h"
#include "lib/str.h"
#include "token.h"

typedef struct Identifier {
    str name;
    Span span;
} Identifier;
LIST(IdentList, Identifier*);
typedef struct Path {
    IdentList elements;
    bool absolute;
    bool ends_in_double_colon;
} Path;
LIST(PathList, Path*);
Path* path_new(bool absolute, IdentList elements);
void path_append(Path* parent, Identifier* child);
Path* path_join(Path* parent, Path* child);
void fprint_path(FILE* file, Path* path);
typedef struct TypeValue TypeValue;
typedef struct FuncDef FuncDef;
typedef struct GenericKeys GenericKeys;
typedef struct Field {
    Identifier* name;
    TypeValue* type;
} Field;
typedef struct TypeDef {
    Identifier* name;
    GenericKeys* generics;
    str extern_ref;
    Map* fields;
} TypeDef;
LIST(TypeValueList, TypeValue*);

typedef struct GenericKeys {
    Span span;
    IdentList generics;
    StrList generic_use_keys;
    Map* generic_uses;
    Map* resolved;
} GenericKeys;

typedef struct GenericValues {
    Span span;
    TypeValueList generics;
    GenericKeys* generic_type_ctx;
    GenericKeys* generic_func_ctx;
    Map* resolved;
} GenericValues;

typedef struct TypeValue {
    Path* name;
    GenericValues* generics;
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
    EXPR_FIELD_ACCESS,
    EXPR_STRUCT_LITERAL,
    EXPR_C_INTRINSIC
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

typedef struct CIntrinsic {
    Map* type_bindings;
    Map* var_bindings;
    str c_expr;
    TypeValue* ret_ty;
} CIntrinsic;

typedef struct StructFieldLit {
    Identifier* name;
    Expression* value;
} StructFieldLit;
typedef struct StructLiteral {
    TypeValue* type;
    Map* fields;
} StructLiteral;

typedef struct BinOp {
    Expression* lhs;
    Expression* rhs;
    str op;
    Span op_span;
} BinOp;

typedef struct FuncDef FuncDef;
typedef struct FuncCall {
    Path* name;
    ExpressionList arguments;
    GenericValues* generics;
    FuncDef* def;
} FuncCall;

typedef struct Block {
    ExpressionList expressions;
    bool yield_last;
    Span span;
    TypeValue* res;
} Block;

typedef struct FieldAccess {
    Expression* object;
    Identifier* field;
} FieldAccess;

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
    bool no_mangle;
    bool is_variadic;
    GenericKeys* generics;
} FuncDef;

ENUM(ModuleItemType,
    MIT_FUNCTION,
    MIT_STRUCT
);

typedef struct ModuleItem {
    void* item;
    ModuleItemType type;
    Span span;
    bool head_resolved;
} ModuleItem;
#endif