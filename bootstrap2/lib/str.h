#ifndef LIB_STR_H
#define LIB_STR_H

#include "defines.h"
#include "list.h"

str copy_str(str s);

str concat_str(str a, str b);

bool str_contains(str a, str b);
bool str_contains_char(str a, char b);

LIST(StrList, str);

#endif