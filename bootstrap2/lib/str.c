#include "../lib.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

str copy_str(str s) {
    usize len = strlen(s);
    str t = gc_malloc(len + 1);
    memcpy(t, s, len);
    t[len] = '\0';
    return t;
}

str concat_str(str a, str b) {
    usize len_a = strlen(a);
    usize len_b = strlen(b);
    str t = gc_malloc(len_a + len_b + 1);
    memcpy(t, a, len_a);
    memcpy(t + len_a, b, len_b);
    t[len_a + len_b] = '\0';
    return t;
}

bool str_contains(str a, str b) {
    usize len_a = strlen(a);
    usize len_b = strlen(b);
    if (len_b == 0) return true;
    if (len_a == 0 || len_b > len_a) return false;
    for (usize i = 0;i < len_a - len_b + 1;i++) {
        if (strcmp(a + i, b) == 0) return true;
    }
    return false;
}

bool str_contains_char(str a, char b) {
    usize len_a = strlen(a);
    for (usize i = 0;i < len_a;i++) {
        if (a[i] == b) return true;
    }
    return false;
}