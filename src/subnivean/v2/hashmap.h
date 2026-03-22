// hashmap.h - Robin Hood Hash Map for Subnivean
//
// A fast, cache-friendly hash map using Robin Hood hashing.
// Keys are int64_t (can encode coordinates, interned strings, etc.)
// Values are int64_t (can store integers or addresses)

#ifndef SUBNIVEAN_HASHMAP_H
#define SUBNIVEAN_HASHMAP_H

#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// Hash Map Structure
// =============================================================================

typedef struct {
    int64_t key;
    int64_t value;
    uint8_t psl; // Probe Sequence Length (0 = empty slot)
} HashEntry;

typedef struct {
    HashEntry *entries;
    int capacity; // Always power of 2
    int count;    // Number of entries
    int max_psl;  // Track max PSL for iteration optimization
} HashMap;

// =============================================================================
// API
// =============================================================================

// Create a new hash map
HashMap *hashmap_new(void);

// Create with initial capacity hint
HashMap *hashmap_new_with_capacity(int capacity);

// Free a hash map
void hashmap_free(HashMap *map);

// Get value for key. Returns true if found, false if not.
// If found, *value is set to the value.
bool hashmap_get(HashMap *map, int64_t key, int64_t *value);

// Set key to value. Overwrites if key exists.
void hashmap_set(HashMap *map, int64_t key, int64_t value);

// Check if key exists
bool hashmap_has(HashMap *map, int64_t key);

// Delete key. Returns true if key existed.
bool hashmap_del(HashMap *map, int64_t key);

// Get number of entries
int hashmap_len(HashMap *map);

// Get all keys as a newly allocated array. Caller must free.
// Sets *count to number of keys.
int64_t *hashmap_keys(HashMap *map, int *count);

// Clear all entries
void hashmap_clear(HashMap *map);

// =============================================================================
// Iteration
// =============================================================================

// Iterator for traversing map entries
typedef struct {
    HashMap *map;
    int index;
} HashMapIter;

// Initialize iterator
void hashmap_iter_init(HashMapIter *iter, HashMap *map);

// Get next entry. Returns false when exhausted.
bool hashmap_iter_next(HashMapIter *iter, int64_t *key, int64_t *value);

#endif // SUBNIVEAN_HASHMAP_H
