#ifndef LIB_LIST_H
#define LIB_LIST_H

#include "defines.h"

#include <string.h>
#include <stdlib.h>

#define list_append(list, element) list_append_raw((list).elements, (list).length, (list).capacity, element)

#define list_append_raw(elements_ptr, length, capacity, element) ({ \
    if ((length) >= (capacity)) { \
        (capacity) += 1; \
        (capacity) *= 2; \
        (elements_ptr) = gc_realloc(elements_ptr, (capacity) * sizeof(element)/*NOLINT(bugprone-sizeof-expression)*/); \
    } \
    (elements_ptr)[length] = element; \
    (length) += 1; \
})

#define list_swap_remove(list, index) ({ \
    (list).length -= 1; \
    typeof(*((list).elements)) v = (list).elements[index]; \
    if ((index) != (list).length) { \
        (list).elements[index] = (list).elements[(list).length]; \
    } \
    v; \
})

#define list_swap_retain(list, iter_var, predicate) do { \
    for (usize i = 0;i < (list).length;i++) { \
        typeof(*((list).elements)) item_var = (list).elements[i]; \
        if (!(predicate)) { \
            list_swap_remove(list, i); \
            i--; \
        } \
    } \
} while(0)

#define list_map_retain(list, lambda_func) do { \
    for (usize i = 0;i < (list).length;i++) { \
        if (!((lambda_func)(&(list).elements[i]))) { \
            list_swap_remove(list, i); \
            i--; \
        } \
    } \
} while(0)

#define list_find_swap_remove(list, item_var, predicate) ({ \
    typeof(*((list).elements)) element1234567890; \
    for (usize i = 0;i < (list).length;i++) { \
        typeof(*((list).elements)) item_var = (list).elements[i]; \
        if (predicate) { \
            element1234567890 = list_swap_remove(list, i); \
            break; \
        } \
    } \
    element1234567890; \
})

#define list_map(list, lambda_func) do { \
    for (usize i = 0;i < (list).length;i++) { \
        (lambda_func)(&((list).elements[i])); \
    } \
} while(0)

#define list_filter_map(list, item_var, predicate, lambda_func) do { \
    for (usize i = 0;i < (list).length;i++) { \
        typeof(*((list).elements)) item_var = (list).elements[i]; \
        if (predicate) { \
            (lambda_func)(&((list).elements[i])); \
        } \
    } \
} while(0)

#define list_find_map(list, item_var, predicate, lambda_func) do { \
    for (usize i = 0;i < (list).length;i++) { \
        typeof(*((list).elements)) item_var = (list).elements[i]; \
        if (predicate) { \
            (lambda_func)(&((list).elements[i])); \
            break; \
        } \
    } \
} while(0)

#define list_pop(list) ({ \
    (list).length -= 1; \
    (list).elements[(list).length]; \
})

#define list_extend(list, drain_list) ({ \
    for (usize i = 0;i < (drain_list).length;i++) { \
        list_append(list, (drain_list).elements[i]); \
    } \
})

#define list_new() { 0, 0, NULL }

#define LIST(Name, Type) \
    typedef struct { \
        usize length; \
        usize capacity; \
        Type* elements; \
    } Name

#endif