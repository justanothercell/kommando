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

typedef struct GKey GKey;
typedef struct TypeDef TypeDef;
typedef struct TraitDef {
    Identifier* name;
    GKey* self_key;
    TypeDef* self_type;
    TypeValue* self_tv;
    Map* methods;
    GenericKeys* keys;
    Module* module;
    bool head_resolved;
} TraitDef;
LIST(TraitList, TraitDef*);

typedef struct TypeDef {
    Identifier* name;
    GenericKeys* generics;
    str extern_ref;
    IdentList flist;
    Map* fields;
    u32 transpile_state;
    Module* module;
    AnnoList annotations;
    TraitList traits;
    bool head_resolved;
} TypeDef;
LIST(TypeValueList, TypeValue*);
void fprint_td_path(FILE* file, TypeDef* def);
void fprint_type(FILE* file, TypeDef* def);

typedef struct TraitBound {
    TypeValue* bound;
    TraitDef* resolved;
} TraitBound;
LIST(TraitBoundList, TraitBound*);
typedef struct GKey {
    Identifier* name;
    TraitBoundList bounds;
} GKey;
LIST(GKeyList, GKey*);
typedef struct GenericKeys {
    Span span;
    GKeyList generics;
    Map* resolved;
    StrList generic_use_keys;
    Map* generic_uses;
} GenericKeys;
void fprint_generic_keys(FILE* file, GenericKeys* keys);

typedef struct GenericValues {
    Span span;
    TypeValueList generics;
    Map* resolved;
} GenericValues;
void fprint_generic_values(FILE* file, GenericValues* values);

typedef struct GenericUse {
    GenericValues* type_context;
    GenericValues* func_context;
    FuncDef* in_func;
} GenericUse;

typedef struct TypeValue {
    Path* name;
    GenericValues* generics;
    TypeDef* def;
    GenericKeys* ctx;
    Map* trait_impls;
} TypeValue;
void fprint_typevalue(FILE* file, TypeValue* tval);

typedef struct VarBox VarBox;
typedef struct Global Global;
typedef struct Variable {
    Path* path;
    VarBox* box;
    Global* global_ref;
    GenericValues* values;
    Identifier* method_name;
    GenericValues* method_values;
} Variable;

ENUM(ExprType, 
    EXPR_BIN_OP,
    EXPR_BIN_OP_ASSIGN,  // typeof(expr) = BinOp*
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

ENUM(VarState,
    VS_UNUSED,
    VS_COPY,
    VS_MOVED,
    VS_DROPPED
);

typedef struct ModuleItem ModuleItem;
typedef struct VarBox {
    str name;
    usize id;
    TVBox* resolved;
    TypeValue* ty;
    ModuleItem* mi;
    GenericValues* values;
    VarState state;
} VarBox;
LIST(VarList, VarBox*);
LIST(VariableList, Variable*);

typedef struct CIntrinsic {
    TypeValueList type_bindings;
    VariableList var_bindings;
    UsizeList binding_sizes;
    str c_expr;
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
    TypeValue* tv;
    Expression* object;
    Identifier* name;
    ExpressionList arguments;
    GenericValues* generics;
    GenericValues* impl_vals;
    FuncDef* def;
} MethodCall;
typedef struct StaticMethodCall {
    TypeValue* tv;
    Identifier* name;
    ExpressionList arguments;
    GenericValues* generics;
    GenericValues* impl_vals;
    FuncDef* def;
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
    bool is_ref;
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
typedef struct ImplBlock ImplBlock;
typedef struct FuncDef {
    Identifier* name;
    Block* body;
    TypeValue* return_type;
    ArgumentList args;
    GenericKeys* generics;
    GenericKeys* type_generics;
    Module* module;
    AnnoList annotations;
    TypeValue* impl_type;
    TraitDef* trait;
    ImplBlock* impl_block;
    bool trait_def;
    bool no_mangle;
    bool is_variadic;
    bool head_resolved;
    bool untraced;
} FuncDef;

typedef struct Global {
    Identifier* name;
    TypeValue* type;
    AnnoList annotations;
    Module* module;
    Expression* value;
    Token* computed_value;
    bool constant;
} Global;

ENUM(Visibility,
    V_PRIVATE,
    V_LOCAL,
    V_PUBLIC,
);

typedef struct ImplBlock {
    TypeValue* type;
    Map* methods;
    GenericKeys* generics;
    TypeValue* trait_ref;
    TraitDef* trait;
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