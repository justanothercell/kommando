#ifndef RESOLVER_H
#define RESOLVER_H

#include "ast.h"
#include "module.h"
#include "compiler.h"

void resolve(Program* program, CompilerOptions* options);
str gvals_to_key(GenericValues* generics);
str gvals_to_c_key(GenericValues* generics);
str gvals_to_dbg_key(GenericValues* generics);
void fprint_res_tv(FILE* stream, TypeValue* tv);
void try_fprint_res_tv(FILE* stream, TypeValue* tv);
str tfvals_to_key(GenericValues* type_generics, GenericValues* func_generics);
void report_item_cache_stats();
TypeValue* replace_generic(TypeValue* tv, GenericValues* type_ctx, GenericValues* func_ctx, TraitDef* trait, TypeValue* trait_called);
#endif