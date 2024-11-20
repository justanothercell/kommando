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
Path* path_simple(Identifier* name);
void path_append(Path* parent, Identifier* child);
Path* path_join(Path* parent, Path* child);
void fprint_path(FILE* file, Path* path);
typedef struct TypeValue TypeValue;
typedef struct FuncDef FuncDef;
typedef struct Module Module;
typedef struct GenericKeys GenericKeys;
typedef struct Field {
    Identifier* name;
    TypeValue* type;
} Field;
typedef struct TypeDef {
    Identifier* name;
    GenericKeys* generics;
    str extern_ref;
    IdentList flist;
    Map* fields;
    u32 transpile_state;
    Module* module;
    bool head_resolved;
} TypeDef;
LIST(TypeValueList, TypeValue*);
void fprint_td_path(FILE* file, TypeDef* def);

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
    Map* resolved;
} GenericValues;

typedef struct TypeValue {
    Path* name;
    GenericValues* generics;
    TypeDef* def;
    GenericKeys* ctx;
} TypeValue;
void fprint_typevalue(FILE* file, TypeValue* tval);

typedef struct VarBox VarBox;
typedef struct Variable {
    Path* path;
    VarBox* box;
} Variable;

ENUM(ExprType, 
    EXPR_BIN_OP,
    EXPR_FUNC_CALL,
    EXPR_LITERAL,  // typeof(expr) = Token*
    EXPR_BLOCK,
    EXPR_VARIABLE,
    EXPR_LET,
    EXPR_ASSIGN,
    EXPR_CONDITIONAL,
    EXPR_WHILE_LOOP,
    EXPR_RETURN,   // typeof(expr) = Expression*
    EXPR_BREAK,    // typeof(expr) = Expression*
    EXPR_TAKEREF,  // typeof(expr) = Expression*
    EXPR_DEREF,    // typeof(expr) = Expression*
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

typedef struct Assign {
    Expression* asignee;
    Expression* value;
} Assign;

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
    GenericKeys* generics;
    Module* module;
    bool no_mangle;
    bool is_variadic;
    bool head_resolved;
} FuncDef;

typedef struct Import {
    Path* path;
    Module* container;
    bool wildcard;
} Import;

typedef struct ModDef {
    bool pub;
    Identifier* name;
} ModDef;
LIST(ModDefList, ModDef*);

#endif