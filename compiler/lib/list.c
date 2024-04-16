#define list_append(element, list_ptr, length, capacity) { \
    if (length >= capacity) { \
        list_ptr = realloc(list_ptr, (capacity + 1) * 1.5 * sizeof(void*)); \
        capacity += 1; \
        capacity *= 1.5; \
    } \
    list_ptr[length] = element; \
    length += 1; \
}