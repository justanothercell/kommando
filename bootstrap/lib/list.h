#ifndef LIB_LIST_H
#define LIB_LIST_H

#include "defines.h"

#include <string.h>

#define list_append(list_ptr, element) list_append_raw((element), (list_ptr)->elements, (list_ptr)->length, (list_ptr)->capacity)

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

#define list_swap_retain(list_ptr, iter_var, predicate) do { \
    for (usize i = 0;i < (list_ptr)->length;i++) { \
        iter_var = (list_ptr)->elements[i]; \
        if (!(predicate)) { \
            list_swap_remove(list_ptr, i); \
            i--; \
        } \
    } \
} while(0)

#define list_find_swap_remove(list_ptr, item_var, predicate) ({ \
    typeof(*((list_ptr)->elements)) element1234567890; \
    memset(&element1234567890, 0, sizeof(element1234567890)); \
    for (usize i = 0;i < (list_ptr)->length;i++) { \
        item_var = &(list_ptr)->elements[i]; \
        if (predicate) { \
            element1234567890 = list_swap_remove(list_ptr, i); \
            break; \
        } \
    } \
    element1234567890; \
})

#define list_map(list_ptr, item_ref_var, mapper) do { \
    for (usize i = 0;i < (list_ptr)->length;i++) { \
        item_ref_var = &((list_ptr)->elements[i]); \
        mapper; \
    } \
} while(0)

#define list_filter_map(list_ptr, counter, item_var, predicate, mapper) do { \
    for (usize counter = 0;counter < (list_ptr)->length;counter++) { \
        item_var = &(list_ptr)->elements[counter]; \
        if (predicate) { \
            mapper; \
        } \
    } \
} while(0)

#define list_find_map(list_ptr, counter, item_var, predicate, mapper) ({ \
    bool found1234567890 = false; \
    for (usize counter = 0;counter < (list_ptr)->length;counter++) { \
        item_var = &(list_ptr)->elements[counter]; \
        if (predicate) { \
            mapper; \
            found1234567890 = true; \
            break; \
        } \
    } \
    found1234567890; \
})

#define list_contains(list_ptr, counter, item_var, predicate) ({ \
    bool found1234567890 = false; \
    for (usize counter = 0;counter < (list_ptr)->length;counter++) { \
        item_var = (list_ptr)->elements[counter]; \
        if (predicate) { \
            found1234567890 = true; \
            break; \
        } \
    } \
    found1234567890; \
})

#define list_pop(list_ptr) ({ \
    (list_ptr)->length -= 1; \
    (list_ptr)->elements[(list_ptr)->length]; \
})

#define list_foreach(list_ptr, counter, item_def, body) ({ \
    for (usize counter = 0;counter < (list_ptr)->length;counter++) { \
        item_def = (list_ptr)->elements[counter]; \
        body; \
    } \
})

#define list_extend(list_ptr, joinee_ptr) ({ \
    typeof(joinee_ptr) j1234567890 = joinee_ptr; \
    for (usize i = 0;i < j1234567890->length;i++) { \
        list_append((list_ptr), j1234567890->elements[i]); \
    } \
})

#define list_reverse(list_ptr) ({ \
    typeof(list_ptr) l1234567890 = list_ptr; \
    for (usize i = 0;i < (l1234567890)->length / 2;i++) { \
        typeof(*l1234567890->elements) e1234567890 = l1234567890->elements[i]; \
        l1234567890->elements[i] = l1234567890->elements[l1234567890->length - i - 1]; \
        l1234567890->elements[l1234567890->length - i - 1] = e1234567890; \
    } \
})


#define list_new(Type) ((Type){ (usize)0, (usize)0, NULL })

#define LIST(Name, Type) \
    typedef struct { \
        usize length; \
        usize capacity; \
        Type* elements; \
    } Name

LIST(List, void*);
LIST(UsizeList, usize);

#endif