#ifndef RESOLVER_H
#define RESOLVER_H

#include "ast.h"
#include "module.h"

void resolve(Program* program);
str gvals_to_key(GenericValues* generics);
str gvals_to_c_key(GenericValues* generics);

#endif