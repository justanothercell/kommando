#ifndef LIB_STRMAP_H
#define LIB_STRMAP_H

#include "defines.h"
#include "list.h"

typedef struct KVPair {
    str key;
    void* value;
} KVPair;

LIST(Bucket, KVPair);
LIST(BucketList, Bucket);

typedef struct Map {
    BucketList buckets;
    u32 random;
} Map;

Map* map_new();
bool map_contains(Map* map, str key);
void* map_get(Map* map, str key);
void* map_remove(Map* map, str key);
void* map_put(Map* map, str key, void* value);
usize map_size(Map* map);

#define map_foreach(map_ptr, key_name, value_name, map_block) do { \
    for (usize i1234567890 = 0;i1234567890 < (map_ptr)->buckets.length;i1234567890++) { \
        Bucket* bucket1234567890 = &(map_ptr)->buckets.elements[i1234567890]; \
        for (usize j1234567890 = 0;j1234567890 < bucket1234567890->length;j1234567890++) { \
            key_name = bucket1234567890->elements[j1234567890].key; \
            value_name = bucket1234567890->elements[j1234567890].value; \
            map_block; \
        } \
    } \
} while(0)

#endif