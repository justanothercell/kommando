#pragma once

#include "defines.c"
#include <string.h>

#define list_append(list_ptr, element) list_append_raw(element, (list_ptr)->elements, (list_ptr)->length, (list_ptr)->capacity)

#define list_append_raw(element, list_ptr, length, capacity) ({ \
    if ((length) >= (capacity)) { \
        (capacity) += 1; \
        (capacity) *= 2; \
        (list_ptr) = realloc(list_ptr, (capacity) * sizeof(element)/*NOLINT(bugprone-sizeof-expression)*/); \
    } \
    (list_ptr)[length] = element; \
    (length) += 1; \
})

#define list_swap_remove(list_ptr, index) ({ \
    (list_ptr)->length -= 1; \
    typeof(*((list_ptr)->elements)) v = (list_ptr)->elements[index]; \
    if ((index) != (list_ptr)->length) { \
        (list_ptr)->elements[index] = (list_ptr)->elements[(list_ptr)->length]; \
    } \
    v; \
})

#define list_pop(list_ptr) ({ \
    (list_ptr)->length -= 1; \
    (list_ptr)->elements[(list_ptr)->length]; \
})

#define list_foreach(list_ptr, function) ({ \
    for (usize i = 0;i < (list_ptr)->length;i++) { \
        function((list_ptr)->elements[i]); \
    } \
})

#define list_extend(list_ptr, drain_list_ptr) ({ \
    for (usize i = 0;i < (drain_list_ptr)->length;i++) { \
        list_append(list_ptr, (drain_list_ptr)->elements[i]); \
    } \
})

#define list_new() { 0, 0, NULL }

#define LIST(Name, Type) \
    typedef struct { \
        usize length; \
        usize capacity; \
        Type* elements; \
    } Name

LIST(StringList, char*);

str copy_str(str s) {
    usize len = strlen(s);
    str t = malloc(len + 1);
    strcpy(t, s);
    t[len] = '\0';
    return t;
}