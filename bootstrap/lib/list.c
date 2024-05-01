#pragma once

#define list_append(element, list_ptr, length, capacity) do { \
    if ((length) >= (capacity)) { \
        (capacity) += 1; \
        (capacity) *= 2; \
        (list_ptr) = realloc(list_ptr, (capacity) * sizeof(element)/*NOLINT(bugprone-sizeof-expression)*/); \
    } \
    (list_ptr)[length] = element; \
    (length) += 1; \
} while (0)
