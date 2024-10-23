#include <stdio.h>
#include <stdlib.h>

#include "../lib.h"
#include "defines.h"


Map* map_new() {
    Map* map = gc_malloc(sizeof(Map));
    map->buckets = list_new(BucketList);
    for (usize i = 0;i < 256;i++) list_append(&map->buckets, list_new(Bucket));
    map->random = rand();
    return map;
}

void* map_get(Map* map, str key) {
    u32 hash = str_hash_seed(key, map->random);
    Bucket* bucket = &map->buckets.elements[hash % map->buckets.length];
    for (usize i = 0;i < bucket->length;i++) {
        if (str_eq(bucket->elements[i].key, key)) return bucket->elements[i].value;
    }
    return NULL;
}

usize map_size(Map* map) {
    usize size = 0;
    for(usize i = 0;i < map->buckets.length;i++) {
        size += map->buckets.elements[i].length;
    }
    return size;
}

bool map_contains(Map* map, str key) {
    return map_get(map, key) != NULL;
}

static void map_double_buckets(Map* map) {
    BucketList old = map->buckets;
    map->buckets = list_new(BucketList);
    map->random = rand();
    for (usize i = 0;i < old.length * 2;i++) list_append(&map->buckets, list_new(Bucket));
    for (usize i = 0;i < old.length;i++) {
        for (usize j = 0;j < old.elements[i].length;j++) {
            KVPair item = old.elements[i].elements[j];
            map_put(map, item.key, item.value);
        }
    }
}

void* map_remove(Map* map, str key) {
    u32 hash = str_hash_seed(key, map->random);
    Bucket* bucket = &map->buckets.elements[hash % map->buckets.length];
    for (usize i = 0;i < bucket->length;i++) {
        if (str_eq(bucket->elements[i].key, key)) {
            return list_swap_remove(bucket, i).value;
        }
    }
    return NULL;
}

void* map_put(Map* map, str key, void* value) {
    if (value == NULL) return map_remove(map, key);
    u32 hash = str_hash_seed(key, map->random);
    Bucket* bucket = &map->buckets.elements[hash % map->buckets.length];
    for (usize i = 0;i < bucket->length;i++) {
        if (str_eq(bucket->elements[i].key, key)) {
            void* old =  bucket->elements[i].value;
            bucket->elements[i].value = value;
            return old;
        }
    }
    list_append(bucket, ((KVPair) { key, value }));
    if (bucket->length > map->buckets.length) {
        map_double_buckets(map);
    }
    return NULL;
}