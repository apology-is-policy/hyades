// persistent_store.c - Subnivean Persistent Store (Arrays and Maps)
//
// Global store for arrays and maps that need to persist across VM calls
// and be accessible from both interpreter and Subnivean.

#include "persistent_store.h"
#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Internal Structures
// ============================================================================

typedef enum { STORE_ARRAY, STORE_STRING_ARRAY, STORE_MAP } StoreType;

typedef struct {
    int64_t *data;
    int len;
    int cap;
} PersistentArray;

typedef struct {
    char **data; // Array of strdup'd strings
    int len;
    int cap;
} PersistentStringArray;

typedef struct {
    StoreType type;
    union {
        PersistentArray *array;
        PersistentStringArray *string_array;
        HashMap *map;
    };
} StoreEntry;

// ============================================================================
// Global Store
// ============================================================================

static StoreEntry *g_store = NULL;
static int g_store_len = 0;
static int g_store_cap = 0;

// ============================================================================
// Internal Helpers
// ============================================================================

static int store_add_entry(void) {
    if (g_store_len >= g_store_cap) {
        int new_cap = g_store_cap == 0 ? 16 : g_store_cap * 2;
        StoreEntry *new_store = realloc(g_store, new_cap * sizeof(StoreEntry));
        if (!new_store) return -1;
        g_store = new_store;
        g_store_cap = new_cap;
    }
    return g_store_len++;
}

// ============================================================================
// Array Operations
// ============================================================================

int sn_store_create_array(int64_t *elements, int n_elements) {
    int addr = store_add_entry();
    if (addr < 0) return -1;

    PersistentArray *arr = malloc(sizeof(PersistentArray));
    if (!arr) {
        g_store_len--;
        return -1;
    }

    arr->len = n_elements;
    arr->cap = n_elements > 0 ? n_elements : 16;
    arr->data = calloc(arr->cap, sizeof(int64_t));
    if (!arr->data) {
        free(arr);
        g_store_len--;
        return -1;
    }

    if (elements && n_elements > 0) {
        memcpy(arr->data, elements, n_elements * sizeof(int64_t));
    }

    g_store[addr].type = STORE_ARRAY;
    g_store[addr].array = arr;

    return addr;
}

int64_t sn_store_array_get(int addr, int idx) {
    if (addr < 0 || addr >= g_store_len) return 0;
    if (g_store[addr].type != STORE_ARRAY) return 0;
    PersistentArray *arr = g_store[addr].array;
    if (!arr || idx < 0 || idx >= arr->len) return 0;
    return arr->data[idx];
}

bool sn_store_array_set(int addr, int idx, int64_t value) {
    if (addr < 0 || addr >= g_store_len) return false;
    if (g_store[addr].type != STORE_ARRAY) return false;
    PersistentArray *arr = g_store[addr].array;
    if (!arr || idx < 0 || idx >= arr->len) return false;
    arr->data[idx] = value;
    return true;
}

int sn_store_array_len(int addr) {
    if (addr < 0 || addr >= g_store_len) return 0;
    if (g_store[addr].type != STORE_ARRAY) return 0;
    PersistentArray *arr = g_store[addr].array;
    return arr ? arr->len : 0;
}

// ============================================================================
// String Array Operations
// ============================================================================

int sn_store_create_string_array(const char **elements, int n_elements) {
    int addr = store_add_entry();
    if (addr < 0) return -1;

    PersistentStringArray *arr = malloc(sizeof(PersistentStringArray));
    if (!arr) {
        g_store_len--;
        return -1;
    }

    arr->len = n_elements;
    arr->cap = n_elements > 0 ? n_elements : 16;
    arr->data = calloc(arr->cap, sizeof(char *));
    if (!arr->data) {
        free(arr);
        g_store_len--;
        return -1;
    }

    // Copy (strdup) each string
    if (elements && n_elements > 0) {
        for (int i = 0; i < n_elements; i++) {
            arr->data[i] = elements[i] ? strdup(elements[i]) : NULL;
        }
    }

    g_store[addr].type = STORE_STRING_ARRAY;
    g_store[addr].string_array = arr;

    return addr;
}

const char *sn_store_string_array_get(int addr, int idx) {
    if (addr < 0 || addr >= g_store_len) return NULL;
    if (g_store[addr].type != STORE_STRING_ARRAY) return NULL;
    PersistentStringArray *arr = g_store[addr].string_array;
    if (!arr || idx < 0 || idx >= arr->len) return NULL;
    return arr->data[idx];
}

int sn_store_string_array_len(int addr) {
    if (addr < 0 || addr >= g_store_len) return 0;
    if (g_store[addr].type != STORE_STRING_ARRAY) return 0;
    PersistentStringArray *arr = g_store[addr].string_array;
    return arr ? arr->len : 0;
}

bool sn_store_is_string_array(int addr) {
    if (addr < 0 || addr >= g_store_len) return false;
    return g_store[addr].type == STORE_STRING_ARRAY;
}

// ============================================================================
// Map Operations
// ============================================================================

int sn_store_create_map(void) {
    int addr = store_add_entry();
    if (addr < 0) return -1;

    HashMap *map = hashmap_new();
    if (!map) {
        g_store_len--;
        return -1;
    }

    g_store[addr].type = STORE_MAP;
    g_store[addr].map = map;

    return addr;
}

int64_t sn_store_map_get(int addr, int64_t key) {
    if (addr < 0 || addr >= g_store_len) return 0;
    if (g_store[addr].type != STORE_MAP) return 0;
    HashMap *map = g_store[addr].map;
    if (!map) return 0;

    int64_t value;
    if (hashmap_get(map, key, &value)) {
        return value;
    }
    return 0;
}

bool sn_store_map_set(int addr, int64_t key, int64_t value) {
    if (addr < 0 || addr >= g_store_len) return false;
    if (g_store[addr].type != STORE_MAP) return false;
    HashMap *map = g_store[addr].map;
    if (!map) return false;

    hashmap_set(map, key, value);
    return true;
}

bool sn_store_map_has(int addr, int64_t key) {
    if (addr < 0 || addr >= g_store_len) return false;
    if (g_store[addr].type != STORE_MAP) return false;
    HashMap *map = g_store[addr].map;
    if (!map) return false;

    return hashmap_has(map, key);
}

bool sn_store_map_del(int addr, int64_t key) {
    if (addr < 0 || addr >= g_store_len) return false;
    if (g_store[addr].type != STORE_MAP) return false;
    HashMap *map = g_store[addr].map;
    if (!map) return false;

    return hashmap_del(map, key);
}

int sn_store_map_len(int addr) {
    if (addr < 0 || addr >= g_store_len) return 0;
    if (g_store[addr].type != STORE_MAP) return 0;
    HashMap *map = g_store[addr].map;
    return map ? hashmap_len(map) : 0;
}

int sn_store_map_keys(int addr) {
    if (addr < 0 || addr >= g_store_len) return -1;
    if (g_store[addr].type != STORE_MAP) return -1;
    HashMap *map = g_store[addr].map;
    if (!map) return -1;

    int count;
    int64_t *keys = hashmap_keys(map, &count);

    // Create a new array with the keys
    int arr_addr = sn_store_create_array(keys, count);
    free(keys);

    return arr_addr;
}

// ============================================================================
// General Operations
// ============================================================================

void sn_store_free_all(void) {
    for (int i = 0; i < g_store_len; i++) {
        if (g_store[i].type == STORE_ARRAY && g_store[i].array) {
            free(g_store[i].array->data);
            free(g_store[i].array);
        } else if (g_store[i].type == STORE_STRING_ARRAY && g_store[i].string_array) {
            // Free each string
            for (int j = 0; j < g_store[i].string_array->len; j++) {
                free(g_store[i].string_array->data[j]);
            }
            free(g_store[i].string_array->data);
            free(g_store[i].string_array);
        } else if (g_store[i].type == STORE_MAP && g_store[i].map) {
            hashmap_free(g_store[i].map);
        }
    }
    free(g_store);
    g_store = NULL;
    g_store_len = 0;
    g_store_cap = 0;
}

int sn_store_count(void) {
    return g_store_len;
}

bool sn_store_is_map(int addr) {
    if (addr < 0 || addr >= g_store_len) return false;
    return g_store[addr].type == STORE_MAP;
}
