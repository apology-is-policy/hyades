// hashmap.c - Robin Hood Hash Map Implementation

#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Constants
// =============================================================================

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR_PERCENT 80
#define PSL_EMPTY 0

// =============================================================================
// Hash Function (splitmix64 - fast and good distribution)
// =============================================================================

static inline uint64_t hash_key(int64_t key) {
    uint64_t x = (uint64_t)key;
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

// =============================================================================
// Internal Helpers
// =============================================================================

static inline int index_for(HashMap *map, uint64_t hash) {
    return (int)(hash & (uint64_t)(map->capacity - 1));
}

static inline int next_index(HashMap *map, int idx) {
    return (idx + 1) & (map->capacity - 1);
}

static inline int prev_index(HashMap *map, int idx) {
    return (idx - 1 + map->capacity) & (map->capacity - 1);
}

static void hashmap_resize(HashMap *map, int new_capacity);

// =============================================================================
// Creation / Destruction
// =============================================================================

HashMap *hashmap_new(void) {
    return hashmap_new_with_capacity(INITIAL_CAPACITY);
}

HashMap *hashmap_new_with_capacity(int capacity) {
    // Round up to power of 2
    int cap = INITIAL_CAPACITY;
    while (cap < capacity) cap *= 2;

    HashMap *map = malloc(sizeof(HashMap));
    map->entries = calloc(cap, sizeof(HashEntry));
    map->capacity = cap;
    map->count = 0;
    map->max_psl = 0;
    return map;
}

void hashmap_free(HashMap *map) {
    if (map) {
        free(map->entries);
        free(map);
    }
}

// =============================================================================
// Core Operations
// =============================================================================

bool hashmap_get(HashMap *map, int64_t key, int64_t *value) {
    if (map->count == 0) return false;

    uint64_t hash = hash_key(key);
    int idx = index_for(map, hash);
    uint8_t psl = 1;

    while (map->entries[idx].psl != PSL_EMPTY) {
        if (map->entries[idx].psl < psl) {
            // Robin Hood property: if we see an entry with lower PSL,
            // our key can't be here
            return false;
        }
        if (map->entries[idx].key == key) {
            if (value) *value = map->entries[idx].value;
            return true;
        }
        idx = next_index(map, idx);
        psl++;
        if (psl > map->max_psl + 1) return false;
    }

    return false;
}

void hashmap_set(HashMap *map, int64_t key, int64_t value) {
    // Check if we need to resize
    if (map->count * 100 >= map->capacity * LOAD_FACTOR_PERCENT) {
        hashmap_resize(map, map->capacity * 2);
    }

    uint64_t hash = hash_key(key);
    int idx = index_for(map, hash);

    HashEntry entry = {.key = key, .value = value, .psl = 1};

    while (map->entries[idx].psl != PSL_EMPTY) {
        // Check if key already exists
        if (map->entries[idx].key == key) {
            map->entries[idx].value = value;
            return;
        }

        // Robin Hood: swap if our entry is "poorer" (higher PSL)
        if (entry.psl > map->entries[idx].psl) {
            HashEntry tmp = map->entries[idx];
            map->entries[idx] = entry;
            entry = tmp;
        }

        idx = next_index(map, idx);
        entry.psl++;
    }

    // Found empty slot
    map->entries[idx] = entry;
    map->count++;

    if (entry.psl > map->max_psl) {
        map->max_psl = entry.psl;
    }
}

bool hashmap_has(HashMap *map, int64_t key) {
    return hashmap_get(map, key, NULL);
}

bool hashmap_del(HashMap *map, int64_t key) {
    if (map->count == 0) return false;

    uint64_t hash = hash_key(key);
    int idx = index_for(map, hash);
    uint8_t psl = 1;

    // Find the entry
    while (map->entries[idx].psl != PSL_EMPTY) {
        if (map->entries[idx].psl < psl) {
            return false;
        }
        if (map->entries[idx].key == key) {
            // Found it - now backward shift delete
            map->count--;

            // Shift subsequent entries backward
            int curr = idx;
            int next = next_index(map, curr);

            while (map->entries[next].psl > 1) {
                map->entries[curr] = map->entries[next];
                map->entries[curr].psl--;
                curr = next;
                next = next_index(map, next);
            }

            // Clear the last shifted position
            map->entries[curr].psl = PSL_EMPTY;

            return true;
        }
        idx = next_index(map, idx);
        psl++;
    }

    return false;
}

int hashmap_len(HashMap *map) {
    return map->count;
}

// =============================================================================
// Resize
// =============================================================================

static void hashmap_resize(HashMap *map, int new_capacity) {
    HashEntry *old_entries = map->entries;
    int old_capacity = map->capacity;

    map->entries = calloc(new_capacity, sizeof(HashEntry));
    map->capacity = new_capacity;
    map->count = 0;
    map->max_psl = 0;

    // Reinsert all entries
    for (int i = 0; i < old_capacity; i++) {
        if (old_entries[i].psl != PSL_EMPTY) {
            hashmap_set(map, old_entries[i].key, old_entries[i].value);
        }
    }

    free(old_entries);
}

// =============================================================================
// Keys
// =============================================================================

int64_t *hashmap_keys(HashMap *map, int *count) {
    if (map->count == 0) {
        *count = 0;
        return NULL;
    }

    int64_t *keys = malloc(map->count * sizeof(int64_t));
    int n = 0;

    for (int i = 0; i < map->capacity && n < map->count; i++) {
        if (map->entries[i].psl != PSL_EMPTY) {
            keys[n++] = map->entries[i].key;
        }
    }

    *count = n;
    return keys;
}

void hashmap_clear(HashMap *map) {
    memset(map->entries, 0, map->capacity * sizeof(HashEntry));
    map->count = 0;
    map->max_psl = 0;
}

// =============================================================================
// Iteration
// =============================================================================

void hashmap_iter_init(HashMapIter *iter, HashMap *map) {
    iter->map = map;
    iter->index = -1;
}

bool hashmap_iter_next(HashMapIter *iter, int64_t *key, int64_t *value) {
    while (++iter->index < iter->map->capacity) {
        if (iter->map->entries[iter->index].psl != PSL_EMPTY) {
            if (key) *key = iter->map->entries[iter->index].key;
            if (value) *value = iter->map->entries[iter->index].value;
            return true;
        }
    }
    return false;
}
