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

#define map_foreach(map_ptr, function) ({ \
    for (usize i = 0;i < (map_ptr)->buckets.length;i++) { \
        Bucket* bucket1234567890 = &(map_ptr)->buckets.elements[i]; \
        for (usize j = 0;j < bucket1234567890->length;j++) { \
            function(bucket1234567890->elements[j].key, bucket1234567890->elements[j].value); \
        } \
    } \
})

#endif