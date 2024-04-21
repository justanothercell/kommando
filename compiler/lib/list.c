#pragma once

#define list_append(element, list_ptr, length, capacity) do { \
    printf("1: %lld %lld\n", length, capacity); \
    if (length >= (capacity)) { \
        (capacity) += 1; \
        (capacity) *= 2; \
        printf("new size: %lld %lld\n", length, capacity); \
        (list_ptr) = realloc(list_ptr, (capacity) * sizeof(element)/*NOLINT(bugprone-sizeof-expression)*/); \
    } \
    printf("2: %lld %lld\n", length, capacity); \
    list_ptr[(length)] = element; \
    printf("3: %lld %lld\n", length, capacity); \
    (length) += 1; \
} while (0)
