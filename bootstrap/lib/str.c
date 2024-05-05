#include "str.h"

#include "defines.h"
#include <stdlib.h>
#include <string.h>


str copy_str(str s) {
    usize len = strlen(s);
    str t = malloc(len + 1);
    strcpy(t, s);
    t[len] = '\0';
    return t;
}