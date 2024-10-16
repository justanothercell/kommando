#ifndef RESOLVER_H
#define RESOLVER_H

#include "ast.h"
#include "module.h"

void resolve(Program* program);
str funcusage_to_key(FuncUsage* fu);

#endif