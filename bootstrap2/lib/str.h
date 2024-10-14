#ifndef LIB_STR_H
#define LIB_STR_H

#include "defines.h"
#include "list.h"

LIST(StrList, str);
LIST(String, char);

str copy_str(str s);
str copy_slice(str s, usize length);

str concat_str(str a, str b);
void string_append(String* s, str a);
String to_string(str s);

bool str_contains(str a, str b);
bool str_startswith(str a, str b);
bool str_endswith(str a, str b);
bool str_eq(str a, str b);
bool str_contains_char(str a, char b);

FILE* strbuilder_new();
str strbuilder_close();

u32 str_hash(str str);
u32 str_hash_seed(str str, u32 seed);

str read_file_to_string(str path);
str read_to_string(FILE* file);
#define to_str_writer(stream, expr) ({ \
    char* buf1234567890; \
    size_t len1234567890; \
    FILE* stream = open_memstream(&buf1234567890, &len1234567890); \
    expr; \
    fclose(stream); \
    str result1234567890 = copy_str(buf1234567890); \
    raw_free(buf1234567890); \
    result1234567890; \
})

StrList split(str s, char delimeter);
StrList splitn(str s, char delimeter, usize n);
StrList rsplitn(str s, char delimeter, usize n);

#endif