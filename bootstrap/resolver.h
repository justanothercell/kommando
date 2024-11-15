#ifndef RESOLVER_H
#define RESOLVER_H

#include "ast.h"
#include "module.h"

typedef struct VarBox {
    str name;
    usize id;
    TVBox* resolved;
    ModuleItem* mi;
    GenericValues* values;
} VarBox;
LIST(VarList, VarBox*);

void resolve(Program* program);
str gvals_to_key(GenericValues* generics);
str gvals_to_c_key(GenericValues* generics);
void fprint_res_tv(FILE* stream, TypeValue* tv);

void report_cache();

#endif