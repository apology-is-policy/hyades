// persistent_store.h - Subnivean Persistent Store (Arrays and Maps)
//
// Arrays and maps that persist across VM calls and are accessible from both
// the interpreter and Subnivean VM via numeric addresses.

#ifndef SUBNIVEAN_PERSISTENT_STORE_H
#define SUBNIVEAN_PERSISTENT_STORE_H

#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// Integer Arrays
// =============================================================================

// Create a new integer array in the persistent store
// Returns the address (index) of the new array, or -1 on failure
int sn_store_create_array(int64_t *elements, int n_elements);

// Get an element from a stored integer array
// Returns 0 if address or index is invalid
int64_t sn_store_array_get(int addr, int idx);

// Set an element in a stored integer array
// Returns true on success, false if address or index is invalid
bool sn_store_array_set(int addr, int idx, int64_t value);

// Get the length of a stored integer array
// Returns 0 if address is invalid
int sn_store_array_len(int addr);

// =============================================================================
// String Arrays
// =============================================================================

// Create a new string array in the persistent store
// Strings are copied (strdup'd) - caller retains ownership of input
// Returns the address (index) of the new array, or -1 on failure
int sn_store_create_string_array(const char **elements, int n_elements);

// Get a string from a stored string array
// Returns NULL if address or index is invalid
// Returned pointer is owned by store - do not free
const char *sn_store_string_array_get(int addr, int idx);

// Get the length of a stored string array
// Returns 0 if address is invalid
int sn_store_string_array_len(int addr);

// Check if address is a string array
bool sn_store_is_string_array(int addr);

// =============================================================================
// Maps (Robin Hood Hash Tables)
// =============================================================================

// Create a new map in the persistent store
// Returns the address of the new map, or -1 on failure
int sn_store_create_map(void);

// Get a value from a stored map
// Returns 0 if not found; use sn_store_map_has to distinguish from stored 0
int64_t sn_store_map_get(int addr, int64_t key);

// Set a value in a stored map
// Returns true on success, false if address is invalid
bool sn_store_map_set(int addr, int64_t key, int64_t value);

// Check if a key exists in a stored map
// Returns true if key exists, false otherwise
bool sn_store_map_has(int addr, int64_t key);

// Delete a key from a stored map
// Returns true if key existed and was deleted
bool sn_store_map_del(int addr, int64_t key);

// Get the number of entries in a stored map
// Returns 0 if address is invalid
int sn_store_map_len(int addr);

// Get all keys from a stored map as an array address
// Returns address of new array containing keys, or -1 on failure
int sn_store_map_keys(int addr);

// =============================================================================
// General
// =============================================================================

// Free all arrays and maps in the store (call at program end)
void sn_store_free_all(void);

// Get number of items in store (for debugging)
int sn_store_count(void);

// Check if address is a map (vs array)
bool sn_store_is_map(int addr);

#endif // SUBNIVEAN_PERSISTENT_STORE_H
