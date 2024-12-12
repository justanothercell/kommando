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

// Flags and defines should be unique per annotated object, function calls may repeat
ENUM(AnnotationType,
    AT_FLAG,   // #[foo]
    AT_DEFINE, // #[foo="bar"]
    AT_CALL    // #[foo("bar")]
);

typedef struct Annotation {
    Path* path;
    AnnotationType type;
    void* data; // LITERAL or IDENTIFIER token, unused for AT_FLAG
} Annotation;
LIST(AnnoList, Annotation);

typedef struct TypeDef {
    Identifier* name;
    GenericKeys* generics;
    str extern_ref;
    IdentList flist;
    Map* fields;
    u32 transpile_state;
    Module* module;
    AnnoList annotations;
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

typedef struct GenericUse {
    GenericValues* type_context;
    GenericValues* func_context;
} GenericUse;

typedef struct TypeValue {
    Path* name;
    GenericValues* generics;
    TypeDef* def;
    GenericKeys* ctx;
} TypeValue;
void fprint_typevalue(FILE* file, TypeValue* tval);

typedef struct VarBox VarBox;
typedef struct Static Static;
typedef struct Variable {
    Path* path;
    VarBox* box;
    Static* static_ref;
    GenericValues* values;
    Identifier* method_name;
    GenericValues* method_values;
} Variable;

ENUM(ExprType, 
    EXPR_BIN_OP,
    EXPR_FUNC_CALL,
    EXPR_METHOD_CALL,
    EXPR_STATIC_METHOD_CALL,
    EXPR_DYN_RAW_CALL,
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
typedef struct DynRawCall {
    Expression* callee;
    ExpressionList args;
} DynRawCall;
typedef struct MethodImpl MethodImpl;
typedef struct MethodCall {
    Expression* object;
    Identifier* name;
    ExpressionList arguments;
    GenericValues* generics;
    GenericValues* impl_vals;
    MethodImpl* def;
} MethodCall;
typedef struct StaticMethodCall {
    TypeValue* tv;
    Identifier* name;
    ExpressionList arguments;
    GenericValues* generics;
    GenericValues* impl_vals;
    MethodImpl* def;
} StaticMethodCall;
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
    AnnoList annotations;
    bool no_mangle;
    bool is_variadic;
    bool head_resolved;
    TypeValue* mt;
} FuncDef;

typedef struct Static {
    Identifier* name;
    TypeValue* type;
    AnnoList annotations;
    Module* module;
} Static;

ENUM(Visibility,
    V_PRIVATE,
    V_LOCAL,
    V_PUBLIC,
);

typedef struct ImplBlock {
    TypeValue* type;
    Map* methods;
    GenericKeys* generics;
} ImplBlock;
LIST(ImplList, ImplBlock*);

typedef struct Import {
    Path* path;
    Module* container;
    Identifier* alias;
    Visibility vis;
    bool wildcard;
} Import;

typedef struct ModDef {
    Visibility vis;
    Identifier* name;
} ModDef;
LIST(ModDefList, ModDef*);

#endif