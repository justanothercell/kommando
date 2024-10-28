#include "../lib.h"
#include "exit.h"
#include "list.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

str copy_str(str s) {
    usize len = strlen(s);
    str t = gc_malloc(len + 1);
    memcpy(t, s, len);
    t[len] = '\0';
    return t;
}

str copy_slice(str s, usize length) {
    str t = gc_malloc(length + 1);
    memcpy(t, s, length);
    t[length] = '\0';
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

void string_append(String* s, str a) {
    String r = to_string(a);
    list_extend(s, &r);
}

String to_string(str s) {
    String string = list_new(String);
    string.length = strlen(s);
    string.elements = s;
    return string;
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

bool str_startswith(str a, str b) {
    usize len_a = strlen(a);
    usize len_b = strlen(b);
    if (len_b == 0) return true;
    if (len_a == 0 || len_b > len_a) return false;
    return strncmp(a, b, len_b) == 0;
}

bool str_endswith(str a, str b) {
    usize len_a = strlen(a);
    usize len_b = strlen(b);
    if (len_b == 0) return true;
    if (len_a == 0 || len_b > len_a) return false;
    return strncmp(a + (len_a - len_b), b, len_b) == 0;
}

bool str_eq(str a, str b) {
    return strcmp(a, b) == 0;
}

bool str_contains_char(str a, char b) {
    usize len_a = strlen(a);
    for (usize i = 0;i < len_a;i++) {
        if (a[i] == b) return true;
    }
    return false;
}

u32 str_hash(str str) {
    return str_hash_seed(str, 0);
}

u32 str_hash_seed(str str, u32 seed) {
    u32 hash = 5381;

    hash = ((hash << 5) + hash) + seed % 256;
    hash = ((hash << 5) + hash) + (seed >> 8) % 256;
    hash = ((hash << 5) + hash) + (seed >> 16) % 256;
    hash = ((hash << 5) + hash) + (seed >> 24) % 256;

    char c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

str read_to_string(FILE* file) {
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *string = gc_malloc(fsize + 1);
    fread(string, fsize, 1, file);

    string[fsize] = 0;
    
    return string;
}

str read_file_to_string(str path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) panic("Unable to read file %s", path);
    str r = read_to_string(f);
    fclose(f);
    return r;   
}

StrList split(str s, char delimeter) {
    StrList list = list_new(StrList);
    usize start = 0;
    usize length = strlen(s);
    for (usize i = 0;i < length;i++) {
        if (s[i] == delimeter) {
            list_append(&list, copy_slice(s+start, i - start));
            start = i + 1;
        }
    }
    if (start != length) list_append(&list, copy_slice(s+start, length - start));
    return list;
}

StrList splitn(str s, char delimeter, usize n) {
    StrList list = list_new(StrList);
    usize start = 0;
    usize length = strlen(s);
    for (usize i = 0;i < length && n > 0;i++) {
        if (s[i] == delimeter) {
            list_append(&list, copy_slice(s+start, i - start));
            start = i + 1;
            n -= 1;
        }
    }
    if (start != length) list_append(&list, copy_slice(s+start, length - start));
    return list;
}

StrList rsplitn(str s, char delimeter, usize n) {
    StrList list = list_new(StrList);
    usize length = strlen(s);
    usize end = length - 1;
    for (usize i = length - 1;i > 0 && n > 0;i--) {
        if (s[i] == delimeter) {
            list_append(&list, copy_slice(s+i+1, end - i + 1));
            end = i;
            n -= 1;
        }
    }
    if (end != 0) list_append(&list, copy_slice(s, end + 1));
    list_reverse(&list);
    return list;
}