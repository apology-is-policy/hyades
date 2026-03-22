// label_library.c - Label library implementation

#include "label_library.h"
#include "cassilda.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Platform-specific includes
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#define stat _stat
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define access _access
#define R_OK 4
#define PATH_SEP "\\"
#else
#include <dirent.h>
#include <unistd.h>
#define PATH_SEP "/"
#endif

static inline bool is_absolute_path(const char *p) {
#ifdef _WIN32
    // C:\ or C:/ or UNC \\server
    return (p[0] && p[1] == ':') || (p[0] == '\\' && p[1] == '\\');
#else
    return p[0] == '/';
#endif
}

// Simple hash function for config/cache validation (doesn't need to be cryptographic)
// Using FNV-1a hash which is fast and has good distribution
#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL

static uint64_t fnv1a_hash(const char *data, size_t len) {
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)(unsigned char)data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

// ============================================================================
// Utilities
// ============================================================================

static char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

static void str_trim(char *s) {
    if (!s) return;

    // Trim leading whitespace
    char *start = s;
    while (*start && isspace(*start)) start++;

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    // Trim trailing whitespace
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace(*end)) {
        *end = '\0';
        end--;
    }
}

bool library_can_read_file(const char *path) {
    return access(path, R_OK) == 0;
}

bool library_get_file_info(const char *path, time_t *mtime, size_t *size) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    if (mtime) *mtime = st.st_mtime;
    if (size) *size = st.st_size;
    return true;
}

char *library_get_directory(const char *file_path) {
    if (!file_path) return NULL;

    // Find last separator (either / or \ on Windows)
    const char *last_slash = strrchr(file_path, '/');
#ifdef _WIN32
    const char *last_backslash = strrchr(file_path, '\\');
    if (!last_slash || (last_backslash && last_backslash > last_slash))
        last_slash = last_backslash;
#endif
    if (!last_slash) {
        return str_dup(".");
    }

    size_t len = last_slash - file_path;
    if (len == 0) len = 1;

    char *dir = malloc(len + 1);
    memcpy(dir, file_path, len);
    dir[len] = '\0';
    return dir;
}

char *library_compute_hash(const char *data) {
    uint64_t hash = fnv1a_hash(data, strlen(data));

    // Convert to hex string (16 hex digits for 64-bit hash)
    char *hex = malloc(17);
    sprintf(hex, "%016llx", (unsigned long long)hash);
    return hex;
}

// Get absolute path
static char *get_absolute_path(const char *path) {
#ifdef _WIN32
    char *abs_path = _fullpath(NULL, path, PATH_MAX);
    return abs_path; // _fullpath returns NULL on error
#else
    char *abs_path = realpath(path, NULL);
    return abs_path; // realpath returns NULL on error
#endif
}

// Check if path is a directory
static bool is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

// ============================================================================
// LibraryConfig
// ============================================================================

void library_config_init(LibraryConfig *config) {
    memset(config, 0, sizeof(LibraryConfig));
    config->cache_path = str_dup(".cassilda-cache");
    config->include_subdirs = false;
}

void library_config_free(LibraryConfig *config) {
    for (int i = 0; i < config->n_library_files; i++) {
        free(config->library_files[i]);
    }
    free(config->library_files);

    for (int i = 0; i < config->n_library_dirs; i++) {
        free(config->library_dirs[i]);
    }
    free(config->library_dirs);

    for (int i = 0; i < config->n_target_prefix; i++) {
        free(config->target_prefix_map[i].extension);
        free(config->target_prefix_map[i].value);
    }
    free(config->target_prefix_map);

    free(config->cache_path);
    memset(config, 0, sizeof(LibraryConfig));
}

// Simple JSON parser for our config format
// We'll parse a very limited subset of JSON - just what we need
bool library_config_parse(LibraryConfig *config, const char *json_path, char *error_msg,
                          int error_size) {
    FILE *f = fopen(json_path, "r");
    if (!f) {
        snprintf(error_msg, error_size, "Cannot open config file: %s", json_path);
        return false;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json = malloc(size + 1);
    size_t bytes_read = fread(json, 1, size, f);
    json[bytes_read] = '\0';
    fclose(f);

    // Parse JSON (simple multi-line aware parser)
    char *p = json;

// Helper: Find the next occurrence of a key and return pointer after it
#define FIND_KEY(key) strstr(p, key)

    // Parse "library_files": [...]
    char *library_files_key = strstr(json, "\"library_files\"");
    if (library_files_key) {
        char *bracket = strchr(library_files_key, '[');
        if (bracket) {
            char *close_bracket = strchr(bracket, ']');
            if (close_bracket) {
                // Parse all quoted strings between [ and ]
                char *item = bracket;
                while (item < close_bracket && (item = strchr(item, '"')) != NULL) {
                    item++; // Skip opening quote
                    char *end = strchr(item, '"');
                    if (!end || end > close_bracket) break;

                    size_t len = end - item;
                    char *file = malloc(len + 1);
                    memcpy(file, item, len);
                    file[len] = '\0';

                    config->library_files = realloc(config->library_files,
                                                    (config->n_library_files + 1) * sizeof(char *));
                    config->library_files[config->n_library_files++] = file;

                    item = end + 1;
                }
            }
        }
    }

    // Parse "library_dirs": [...]
    char *library_dirs_key = strstr(json, "\"library_dirs\"");
    if (library_dirs_key) {
        char *bracket = strchr(library_dirs_key, '[');
        if (bracket) {
            char *close_bracket = strchr(bracket, ']');
            if (close_bracket) {
                // Parse all quoted strings between [ and ]
                char *item = bracket;
                while (item < close_bracket && (item = strchr(item, '"')) != NULL) {
                    item++; // Skip opening quote
                    char *end = strchr(item, '"');
                    if (!end || end > close_bracket) break;

                    size_t len = end - item;
                    char *dir = malloc(len + 1);
                    memcpy(dir, item, len);
                    dir[len] = '\0';

                    config->library_dirs = realloc(config->library_dirs,
                                                   (config->n_library_dirs + 1) * sizeof(char *));
                    config->library_dirs[config->n_library_dirs++] = dir;

                    item = end + 1;
                }
            }
        }
    }

    // Parse "include_subdirs": true/false
    char *include_subdirs_key = strstr(json, "\"include_subdirs\"");
    if (include_subdirs_key) {
        config->include_subdirs = (strstr(include_subdirs_key, "true") != NULL);
    }

    // Parse "cache_path": "value"
    char *cache_path_key = strstr(json, "\"cache_path\"");
    if (cache_path_key) {
        char *quote = strchr(cache_path_key, ':');
        if (quote) {
            quote = strchr(quote, '"');
            if (quote) {
                quote++;
                char *end = strchr(quote, '"');
                if (end) {
                    size_t len = end - quote;
                    free(config->cache_path);
                    config->cache_path = malloc(len + 1);
                    memcpy(config->cache_path, quote, len);
                    config->cache_path[len] = '\0';
                }
            }
        }
    }

    // Parse "target_prefix": { ".ext": "value", ... }
    char *target_prefix_key = strstr(json, "\"target_prefix\"");
    if (target_prefix_key) {
        char *brace = strchr(target_prefix_key, '{');
        if (brace) {
            char *close_brace = strchr(brace, '}');
            if (close_brace) {
                // Parse all "key": "value" pairs between { and }
                char *p = brace + 1;
                while (p < close_brace) {
                    // Find extension key
                    char *ext_quote = strchr(p, '"');
                    if (!ext_quote || ext_quote >= close_brace) break;
                    ext_quote++;
                    char *ext_end = strchr(ext_quote, '"');
                    if (!ext_end || ext_end >= close_brace) break;

                    size_t ext_len = ext_end - ext_quote;
                    char *extension = malloc(ext_len + 1);
                    memcpy(extension, ext_quote, ext_len);
                    extension[ext_len] = '\0';

                    // Find value
                    char *colon = strchr(ext_end, ':');
                    if (!colon || colon >= close_brace) {
                        free(extension);
                        break;
                    }
                    char *val_quote = strchr(colon, '"');
                    if (!val_quote || val_quote >= close_brace) {
                        free(extension);
                        break;
                    }
                    val_quote++;
                    char *val_end = strchr(val_quote, '"');
                    if (!val_end || val_end >= close_brace) {
                        free(extension);
                        break;
                    }

                    size_t val_len = val_end - val_quote;
                    char *value = malloc(val_len + 1);
                    memcpy(value, val_quote, val_len);
                    value[val_len] = '\0';

                    // Add to map
                    config->target_prefix_map =
                        realloc(config->target_prefix_map,
                                (config->n_target_prefix + 1) * sizeof(ExtensionMapping));
                    config->target_prefix_map[config->n_target_prefix].extension = extension;
                    config->target_prefix_map[config->n_target_prefix].value = value;
                    config->n_target_prefix++;

                    p = val_end + 1;
                }
            }
        }
    }

    free(json);
    return true;
}

// Resolve config paths relative to config file directory
static void library_config_resolve_paths(LibraryConfig *config, const char *config_path) {
    // Get directory containing config file
    char *config_dir = library_get_directory(config_path);
    if (!config_dir) return;

    // Resolve library_files
    for (int i = 0; i < config->n_library_files; i++) {
        char *file = config->library_files[i];
        if (is_absolute_path(file)) continue;

        char abs_path[PATH_MAX];
        snprintf(abs_path, PATH_MAX, "%s" PATH_SEP "%s", config_dir, file);
        char *resolved = get_absolute_path(abs_path);
        if (resolved) {
            free(config->library_files[i]);
            config->library_files[i] = resolved;
        }
    }

    // Resolve library_dirs
    for (int i = 0; i < config->n_library_dirs; i++) {
        char *dir = config->library_dirs[i];
        if (is_absolute_path(dir)) continue;

        char abs_path[PATH_MAX];
        snprintf(abs_path, PATH_MAX, "%s" PATH_SEP "%s", config_dir, dir);
        char *resolved = get_absolute_path(abs_path);
        if (resolved) {
            free(config->library_dirs[i]);
            config->library_dirs[i] = resolved;
        }
    }

    // Resolve cache_path
    if (config->cache_path && !is_absolute_path(config->cache_path)) {
        char abs_path[PATH_MAX];
        snprintf(abs_path, PATH_MAX, "%s" PATH_SEP "%s", config_dir, config->cache_path);
        char *resolved = get_absolute_path(abs_path);
        if (resolved) {
            free(config->cache_path);
            config->cache_path = resolved;
        }
    }

    free(config_dir);
}

// ============================================================================
// Directory Scanning
// ============================================================================

void library_free_file_list(char **files, int n_files) {
    for (int i = 0; i < n_files; i++) {
        free(files[i]);
    }
    free(files);
}

#ifdef _WIN32
// Windows-specific directory scanning
static void scan_directory_recursive(const char *dir, bool recursive, int depth, char ***files,
                                     int *n_files, int *capacity) {
    // Safety: max recursion depth
    if (depth > 10) return;

    char pattern[PATH_MAX];
    snprintf(pattern, PATH_MAX, "%s\\*", dir);

    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(pattern, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;

    do {
        const char *name = find_data.cFileName;

        // Skip . and ..
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        // Build full path
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s\\%s", dir, name);

        // Check if it's a directory
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive) {
                scan_directory_recursive(path, recursive, depth + 1, files, n_files, capacity);
            }
            continue;
        }

        // Check if it's a .cld file
        size_t name_len = strlen(name);
        if (name_len < 4 || strcmp(name + name_len - 4, ".cld") != 0) {
            continue;
        }

        // Add to list
        if (*n_files >= *capacity) {
            *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
            *files = realloc(*files, *capacity * sizeof(char *));
        }

        char *abs_path = get_absolute_path(path);
        if (abs_path && library_can_read_file(abs_path)) {
            (*files)[(*n_files)++] = abs_path;
        } else {
            free(abs_path);
        }
    } while (FindNextFileA(h_find, &find_data));

    FindClose(h_find);
}
#else
// POSIX directory scanning
static void scan_directory_recursive(const char *dir, bool recursive, int depth, char ***files,
                                     int *n_files, int *capacity) {
    // Safety: max recursion depth
    if (depth > 10) return;

    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", dir, entry->d_name);

        // Check if it's a directory
        if (is_directory(path)) {
            if (recursive) {
                scan_directory_recursive(path, recursive, depth + 1, files, n_files, capacity);
            }
            continue;
        }

        // Check if it's a .cld file
        size_t name_len = strlen(entry->d_name);
        if (name_len < 4 || strcmp(entry->d_name + name_len - 4, ".cld") != 0) {
            continue;
        }

        // Add to list
        if (*n_files >= *capacity) {
            *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
            *files = realloc(*files, *capacity * sizeof(char *));
        }

        char *abs_path = get_absolute_path(path);
        if (abs_path && library_can_read_file(abs_path)) {
            (*files)[(*n_files)++] = abs_path;
        } else {
            free(abs_path);
        }
    }

    closedir(d);
}
#endif

char **library_scan_directory(const char *dir, bool recursive, int *n_files, char *error_msg,
                              int error_size) {
    *n_files = 0;

    if (!is_directory(dir)) {
        if (error_msg) {
            snprintf(error_msg, error_size, "Not a directory: %s", dir);
        }
        return NULL;
    }

    char **files = NULL;
    int capacity = 0;

    scan_directory_recursive(dir, recursive, 0, &files, n_files, &capacity);

    return files;
}

// ============================================================================
// Config Discovery
// ============================================================================

char *library_find_config(const char *start_dir, bool search_up) {
    char current_dir[PATH_MAX];

#ifdef _WIN32
    if (!_fullpath(current_dir, start_dir, PATH_MAX)) {
        return NULL;
    }
    const char *path_sep = "\\";
    const char *config_name = ".cassilda.json";
#else
    if (!realpath(start_dir, current_dir)) {
        return NULL;
    }
    const char *path_sep = "/";
    const char *config_name = ".cassilda.json";
#endif

    if (!search_up) {
        // Just check current directory
        char config_path[PATH_MAX];
        snprintf(config_path, PATH_MAX, "%s%s%s", current_dir, path_sep, config_name);
        if (library_can_read_file(config_path)) {
            return get_absolute_path(config_path);
        }
        return NULL;
    }

    // Upward search (opt-in)
#ifdef _WIN32
    char *home = getenv("USERPROFILE");
    if (!home) home = getenv("HOME");
#else
    char *home = getenv("HOME");
#endif

#ifndef _WIN32
    // Only do device boundary checking on POSIX systems
    struct stat start_stat;
    stat(current_dir, &start_stat);
    dev_t start_device = start_stat.st_dev;
#endif

    while (true) {
        // Try .cassilda.json at current level
        char config_path[PATH_MAX];
        snprintf(config_path, PATH_MAX, "%s%s%s", current_dir, path_sep, config_name);

        if (library_can_read_file(config_path)) {
            return get_absolute_path(config_path);
        }

        // Stop conditions
#ifdef _WIN32
        // On Windows, check for drive root (e.g., "C:\")
        if (strlen(current_dir) <= 3 && current_dir[1] == ':') break;
#else
        if (strcmp(current_dir, "/") == 0) break; // Filesystem root
#endif
        if (home && strcmp(current_dir, home) == 0) break; // Home directory

        // Check for .git directory (project root marker)
        char git_path[PATH_MAX];
        snprintf(git_path, PATH_MAX, "%s%s.git", current_dir, path_sep);
        if (is_directory(git_path)) break;

#ifndef _WIN32
        // Check filesystem boundary (POSIX only)
        struct stat current_stat;
        if (stat(current_dir, &current_stat) != 0) break;
        if (current_stat.st_dev != start_device) break;
#endif

        // Move to parent
        char parent[PATH_MAX];
        snprintf(parent, PATH_MAX, "%s%s..", current_dir, path_sep);

#ifdef _WIN32
        if (!_fullpath(current_dir, parent, PATH_MAX)) break;
#else
        if (!realpath(parent, current_dir)) break;
#endif

        // Safety: check if we can read parent
        if (!library_can_read_file(current_dir)) break;
    }

    return NULL;
}

// ============================================================================
// Label Index (hash table for fast lookup)
// ============================================================================

#define LABEL_INDEX_SIZE 256

typedef struct LabelEntry {
    char *label;
    char *file_path;
    int line;
    struct LabelEntry *next;
} LabelEntry;

struct LabelIndex {
    LabelEntry *buckets[LABEL_INDEX_SIZE];
};

static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % LABEL_INDEX_SIZE;
}

LabelIndex *label_index_create(void) {
    LabelIndex *index = calloc(1, sizeof(LabelIndex));
    return index;
}

void label_index_free(LabelIndex *index) {
    if (!index) return;

    for (int i = 0; i < LABEL_INDEX_SIZE; i++) {
        LabelEntry *entry = index->buckets[i];
        while (entry) {
            LabelEntry *next = entry->next;
            free(entry->label);
            free(entry->file_path);
            free(entry);
            entry = next;
        }
    }
    free(index);
}

void label_index_add(LabelIndex *index, const char *label, const char *file_path, int line) {
    unsigned int bucket = hash_string(label);

    LabelEntry *entry = malloc(sizeof(LabelEntry));
    entry->label = str_dup(label);
    entry->file_path = str_dup(file_path);
    entry->line = line;
    entry->next = index->buckets[bucket];
    index->buckets[bucket] = entry;
}

bool label_index_lookup(const LabelIndex *index, const char *label, const char **file_path,
                        int *line) {
    unsigned int bucket = hash_string(label);
    LabelEntry *entry = index->buckets[bucket];

    while (entry) {
        if (strcmp(entry->label, label) == 0) {
            if (file_path) *file_path = entry->file_path;
            if (line) *line = entry->line;
            return true;
        }
        entry = entry->next;
    }

    return false;
}

void label_index_get_all(const LabelIndex *index, char ***labels, int *n_labels) {
    // Count labels
    int count = 0;
    for (int i = 0; i < LABEL_INDEX_SIZE; i++) {
        LabelEntry *entry = index->buckets[i];
        while (entry) {
            count++;
            entry = entry->next;
        }
    }

    *n_labels = count;
    *labels = malloc(count * sizeof(char *));

    int idx = 0;
    for (int i = 0; i < LABEL_INDEX_SIZE; i++) {
        LabelEntry *entry = index->buckets[i];
        while (entry) {
            (*labels)[idx++] = str_dup(entry->label);
            entry = entry->next;
        }
    }
}

// ============================================================================
// Label Cache
// ============================================================================

void label_cache_init(LabelCache *cache) {
    memset(cache, 0, sizeof(LabelCache));
    cache->index = label_index_create();
}

void label_cache_free(LabelCache *cache) {
    free(cache->config_hash);

    for (int i = 0; i < cache->n_files; i++) {
        TrackedFile *tf = &cache->files[i];
        free(tf->path);
        for (int j = 0; j < tf->n_labels; j++) {
            free(tf->labels[j]);
        }
        free(tf->labels);
    }
    free(cache->files);

    label_index_free(cache->index);
    memset(cache, 0, sizeof(LabelCache));
}

// Helper: Parse a .cld file and extract label names
static bool parse_cld_labels(const char *file_path, char ***labels, int *n_labels, char *error_msg,
                             int error_size) {
    FILE *f = fopen(file_path, "r");
    if (!f) {
        if (error_msg) {
            snprintf(error_msg, error_size, "Cannot read %s", file_path);
        }
        return false;
    }

    *labels = NULL;
    *n_labels = 0;
    int capacity = 0;

    // First pass: find #comment_char directive
    char *comment_char = NULL;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        str_trim(line);
        if (strncmp(line, "#comment_char", 13) == 0) {
            // Parse the string after #comment_char
            const char *p = line + 13;
            while (*p && isspace(*p)) p++;
            if (*p == '"') {
                p++;
                const char *end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    comment_char = malloc(len + 1);
                    memcpy(comment_char, p, len);
                    comment_char[len] = '\0';
                }
            }
            break;
        }
    }

    // Default to empty comment char for .cld files
    if (!comment_char) {
        comment_char = str_dup("");
    }

    size_t comment_len = strlen(comment_char);

    // Second pass: find labels
    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        str_trim(line);

        // Skip lines that don't start with comment_char (if set)
        if (comment_len > 0) {
            if (strncmp(line, comment_char, comment_len) != 0) {
                continue;
            }
            // Skip past comment char
            const char *content = line + comment_len;
            while (*content && isspace(*content)) content++;

            // Look for @label on this line
            if (strncmp(content, "@label", 6) == 0) {
                char *name = (char *)content + 6;
                while (*name && isspace(*name)) name++;

                if (*name) {
                    // Extract label name (until whitespace or end)
                    char *end = name;
                    while (*end && !isspace(*end)) end++;
                    *end = '\0';

                    // Add to list
                    if (*n_labels >= capacity) {
                        capacity = (capacity == 0) ? 8 : (capacity * 2);
                        *labels = realloc(*labels, capacity * sizeof(char *));
                    }
                    (*labels)[(*n_labels)++] = str_dup(name);
                }
            }
        } else {
            // No comment char - look for @label anywhere
            if (strncmp(line, "@label", 6) == 0) {
                char *name = line + 6;
                while (*name && isspace(*name)) name++;

                if (*name) {
                    // Extract label name (until whitespace or end)
                    char *end = name;
                    while (*end && !isspace(*end)) end++;
                    *end = '\0';

                    // Add to list
                    if (*n_labels >= capacity) {
                        capacity = (capacity == 0) ? 8 : (capacity * 2);
                        *labels = realloc(*labels, capacity * sizeof(char *));
                    }
                    (*labels)[(*n_labels)++] = str_dup(name);
                }
            }
        }
    }

    free(comment_char);
    fclose(f);
    return true;
}

// Build cache from a list of .cld files
static bool build_cache_from_files(LabelCache *cache, char **files, int n_files,
                                   const char *config_hash) {
    label_cache_free(cache);
    label_cache_init(cache);

    cache->config_hash = str_dup(config_hash);
    cache->generated = time(NULL);
    cache->files = malloc(n_files * sizeof(TrackedFile));
    cache->files_capacity = n_files;

    for (int i = 0; i < n_files; i++) {
        TrackedFile *tf = &cache->files[cache->n_files];
        tf->path = str_dup(files[i]);

        // Get file info
        if (!library_get_file_info(files[i], &tf->mtime, &tf->size)) {
            continue; // Skip files we can't stat
        }

        // Parse labels
        char error[256];
        if (!parse_cld_labels(files[i], &tf->labels, &tf->n_labels, error, sizeof(error))) {
            continue; // Skip files we can't parse
        }

        // Add to index
        for (int j = 0; j < tf->n_labels; j++) {
            label_index_add(cache->index, tf->labels[j], tf->path, 0);
        }

        cache->n_files++;
    }

    return true;
}

// Serialize cache to JSON
bool label_cache_save(const LabelCache *cache, const char *cache_path, char *error_msg,
                      int error_size) {
    FILE *f = fopen(cache_path, "w");
    if (!f) {
        if (error_msg) {
            snprintf(error_msg, error_size, "Cannot write cache: %s", cache_path);
        }
        return false;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"config_hash\": \"%s\",\n", cache->config_hash ? cache->config_hash : "");
    fprintf(f, "  \"generated\": %ld,\n", (long)cache->generated);
    fprintf(f, "  \"tracked_files\": [\n");

    for (int i = 0; i < cache->n_files; i++) {
        TrackedFile *tf = &cache->files[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"path\": \"%s\",\n", tf->path);
        fprintf(f, "      \"mtime\": %ld,\n", (long)tf->mtime);
        fprintf(f, "      \"size\": %zu,\n", tf->size);
        fprintf(f, "      \"labels\": [");
        for (int j = 0; j < tf->n_labels; j++) {
            fprintf(f, "\"%s\"", tf->labels[j]);
            if (j < tf->n_labels - 1) fprintf(f, ", ");
        }
        fprintf(f, "]\n");
        fprintf(f, "    }%s\n", (i < cache->n_files - 1) ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

// Load and parse cache from JSON
bool label_cache_load(LabelCache *cache, const char *cache_path, char *error_msg, int error_size) {
    FILE *f = fopen(cache_path, "r");
    if (!f) {
        if (error_msg) {
            snprintf(error_msg, error_size, "Cache file not found: %s", cache_path);
        }
        return false;
    }

    label_cache_init(cache);

    char line[2048];
    TrackedFile *current_file = NULL;
    bool in_tracked_files = false;

    while (fgets(line, sizeof(line), f)) {
        str_trim(line);

        // Track when we enter the tracked_files array
        if (strstr(line, "\"tracked_files\"")) {
            in_tracked_files = true;
        }

        // Parse config_hash
        if (strstr(line, "\"config_hash\"")) {
            char *quote = strchr(line, ':');
            if (quote) {
                quote = strchr(quote, '"');
                if (quote) {
                    quote++;
                    char *end = strchr(quote, '"');
                    if (end) {
                        *end = '\0';
                        cache->config_hash = str_dup(quote);
                    }
                }
            }
        }
        // Parse generated timestamp
        else if (strstr(line, "\"generated\"")) {
            sscanf(line, " \"generated\": %ld", (long *)&cache->generated);
        }
        // Start of a tracked file object
        else if (strcmp(line, "{") == 0 && in_tracked_files) {
            if (cache->n_files >= cache->files_capacity) {
                cache->files_capacity =
                    (cache->files_capacity == 0) ? 8 : (cache->files_capacity * 2);
                cache->files = realloc(cache->files, cache->files_capacity * sizeof(TrackedFile));
            }
            current_file = &cache->files[cache->n_files++];
            memset(current_file, 0, sizeof(TrackedFile));
        }
        // Parse file path
        else if (current_file && strstr(line, "\"path\"")) {
            char *quote = strchr(line, ':');
            if (quote) {
                quote = strchr(quote, '"');
                if (quote) {
                    quote++;
                    char *end = strchr(quote, '"');
                    if (end) {
                        *end = '\0';
                        current_file->path = str_dup(quote);
                    }
                }
            }
        }
        // Parse labels array
        else if (current_file && strstr(line, "\"labels\"")) {
            char *bracket = strchr(line, '[');
            if (bracket) {
                char *item = bracket;
                while ((item = strchr(item, '"')) != NULL) {
                    item++;
                    char *end = strchr(item, '"');
                    if (!end) break;
                    *end = '\0';

                    // Add label
                    current_file->labels = realloc(current_file->labels,
                                                   (current_file->n_labels + 1) * sizeof(char *));
                    current_file->labels[current_file->n_labels++] = str_dup(item);

                    // Add to index
                    label_index_add(cache->index, item, current_file->path, 0);

                    item = end + 1;
                }
            }
        }
    }

    fclose(f);
    return true;
}

bool label_cache_is_valid(const LabelCache *cache, const LibraryConfig *config) {
    // Check if config hash matches
    char *config_json = malloc(4096);
    config_json[0] = '\0';

    // Build a string representation of config
    strcat(config_json, "files:");
    for (int i = 0; i < config->n_library_files; i++) {
        strcat(config_json, config->library_files[i]);
        strcat(config_json, ";");
    }
    strcat(config_json, "dirs:");
    for (int i = 0; i < config->n_library_dirs; i++) {
        strcat(config_json, config->library_dirs[i]);
        strcat(config_json, ";");
    }

    char *new_hash = library_compute_hash(config_json);
    free(config_json);

    bool hash_matches = (cache->config_hash && strcmp(cache->config_hash, new_hash) == 0);
    free(new_hash);

    if (!hash_matches) return false;

    // Check if any tracked file has changed
    for (int i = 0; i < cache->n_files; i++) {
        TrackedFile *tf = &cache->files[i];
        time_t mtime;
        size_t size;

        if (!library_get_file_info(tf->path, &mtime, &size)) {
            return false; // File disappeared or can't be read
        }

        if (mtime != tf->mtime || size != tf->size) {
            return false; // File changed
        }
    }

    return true;
}

// ============================================================================
// Library Context
// ============================================================================

void library_options_init(LibraryOptions *opts) {
    memset(opts, 0, sizeof(LibraryOptions));
}

void library_context_init(LibraryContext *ctx, const LibraryOptions *opts) {
    memset(ctx, 0, sizeof(LibraryContext));
    if (opts) {
        ctx->options = *opts;
    }
    library_config_init(&ctx->config);
    label_cache_init(&ctx->cache);
}

void library_context_free(LibraryContext *ctx) {
    library_config_free(&ctx->config);
    label_cache_free(&ctx->cache);
    free(ctx->config_path);
    free(ctx->cache_path);
    memset(ctx, 0, sizeof(LibraryContext));
}

bool library_discover(LibraryContext *ctx, char *error_msg, int error_size) {
    // Step 1: Determine starting directory for config search
    char *start_dir = NULL;
    char *cwd = NULL;

    // Prefer to start from the directory of the processed file
    if (ctx->options.processed_file) {
        start_dir = library_get_directory(ctx->options.processed_file);
    }

    // Fallback to current working directory
    if (!start_dir) {
        cwd = getcwd(NULL, 0);
        start_dir = cwd;
    }

    if (!start_dir) {
        if (error_msg) snprintf(error_msg, error_size, "Cannot determine starting directory");
        return false;
    }

    // Step 2: Find config file
    if (ctx->options.config_path) {
        // Explicit config path
        ctx->config_path = get_absolute_path(ctx->options.config_path);
    } else {
        // Search for config starting from the file's directory
        ctx->config_path = library_find_config(start_dir, ctx->options.find_config);
    }

    // Clean up start_dir if it's not the same as cwd
    if (start_dir != cwd) {
        free(start_dir);
    }

    // Step 3: Load config if found
    if (ctx->config_path) {
        ctx->has_config = true;
        if (!library_config_parse(&ctx->config, ctx->config_path, error_msg, error_size)) {
            if (cwd) free(cwd);
            return false;
        }

        // Resolve relative paths in config to be relative to config file's directory
        library_config_resolve_paths(&ctx->config, ctx->config_path);

        if (ctx->options.verbose) {
            printf("Found config: %s\n", ctx->config_path);
            printf("Config has %d library_dirs, %d library_files\n", ctx->config.n_library_dirs,
                   ctx->config.n_library_files);
            for (int i = 0; i < ctx->config.n_library_dirs; i++) {
                printf("  library_dir[%d]: %s\n", i, ctx->config.library_dirs[i]);
            }
        }
    }

    // Get current working directory for fallback scanning
    if (!cwd) {
        cwd = getcwd(NULL, 0);
    }

    // Step 4: Determine cache path
    if (ctx->has_config && ctx->config.cache_path) {
        ctx->cache_path = str_dup(ctx->config.cache_path);
    } else {
        ctx->cache_path = str_dup(".cassilda-cache");
    }

    // Step 5: Try to load cache
    bool cache_loaded = false;
    if (!ctx->options.no_cache && !ctx->options.rebuild_cache) {
        char cache_error[256];
        cache_loaded =
            label_cache_load(&ctx->cache, ctx->cache_path, cache_error, sizeof(cache_error));

        if (cache_loaded) {
            // Check if cache is still valid
            if (!label_cache_is_valid(&ctx->cache, &ctx->config)) {
                if (ctx->options.verbose) {
                    printf("Cache invalid, rebuilding...\n");
                }
                cache_loaded = false;
                label_cache_free(&ctx->cache);
                label_cache_init(&ctx->cache);
            } else if (ctx->options.verbose) {
                printf("Using cache: %s (%d files, %d labels)\n", ctx->cache_path,
                       ctx->cache.n_files,
                       0); // TODO: count labels
            }
        }
    }

    // Step 6: Build cache if needed
    if (!cache_loaded && (ctx->has_config || ctx->options.rebuild_cache)) {
        // Collect all .cld files to scan
        char **files = NULL;
        int n_files = 0;
        int capacity = 0;

        // Add explicit library files
        for (int i = 0; i < ctx->config.n_library_files; i++) {
            char *abs = get_absolute_path(ctx->config.library_files[i]);
            if (abs && library_can_read_file(abs)) {
                if (n_files >= capacity) {
                    capacity = (capacity == 0) ? 16 : (capacity * 2);
                    files = realloc(files, capacity * sizeof(char *));
                }
                files[n_files++] = abs;
            } else {
                free(abs);
            }
        }

        // Scan library directories
        if (ctx->options.verbose && ctx->config.n_library_dirs > 0) {
            printf("Scanning %d library director%s from config\n", ctx->config.n_library_dirs,
                   ctx->config.n_library_dirs == 1 ? "y" : "ies");
        }
        for (int i = 0; i < ctx->config.n_library_dirs; i++) {
            if (ctx->options.verbose) {
                printf("  Scanning library dir: %s\n", ctx->config.library_dirs[i]);
            }
            int dir_n_files;
            char scan_error[256];
            char **dir_files =
                library_scan_directory(ctx->config.library_dirs[i], ctx->config.include_subdirs,
                                       &dir_n_files, scan_error, sizeof(scan_error));
            if (dir_files) {
                if (ctx->options.verbose) {
                    printf("    Found %d file%s\n", dir_n_files, dir_n_files == 1 ? "" : "s");
                }
                for (int j = 0; j < dir_n_files; j++) {
                    if (n_files >= capacity) {
                        capacity = (capacity == 0) ? 16 : (capacity * 2);
                        files = realloc(files, capacity * sizeof(char *));
                    }
                    files[n_files++] = dir_files[j];
                }
                free(dir_files); // Don't free individual paths, we took ownership
            } else if (ctx->options.verbose) {
                printf("    Error: %s\n", scan_error);
            }
        }

        // Fallback: scan current directory and processed file directory
        if (n_files == 0) {
            int dir_n_files;
            char scan_error[256];

            // Scan CWD
            char **cwd_files =
                library_scan_directory(cwd, false, &dir_n_files, scan_error, sizeof(scan_error));
            if (cwd_files) {
                for (int j = 0; j < dir_n_files; j++) {
                    if (n_files >= capacity) {
                        capacity = (capacity == 0) ? 16 : (capacity * 2);
                        files = realloc(files, capacity * sizeof(char *));
                    }
                    files[n_files++] = cwd_files[j];
                }
                free(cwd_files);
            }

            // Scan processed file directory
            if (ctx->options.processed_file) {
                char *file_dir = library_get_directory(ctx->options.processed_file);
                if (file_dir && strcmp(file_dir, cwd) != 0) {
                    char **file_dir_files = library_scan_directory(file_dir, false, &dir_n_files,
                                                                   scan_error, sizeof(scan_error));
                    if (file_dir_files) {
                        for (int j = 0; j < dir_n_files; j++) {
                            if (n_files >= capacity) {
                                capacity = (capacity == 0) ? 16 : (capacity * 2);
                                files = realloc(files, capacity * sizeof(char *));
                            }
                            files[n_files++] = file_dir_files[j];
                        }
                        free(file_dir_files);
                    }
                }
                free(file_dir);
            }
        }

        // Build cache
        char *config_str = ""; // Simplified for now
        char *hash = library_compute_hash(config_str);
        build_cache_from_files(&ctx->cache, files, n_files, hash);
        free(hash);

        // Save cache
        if (!ctx->options.no_cache) {
            label_cache_save(&ctx->cache, ctx->cache_path, error_msg, error_size);
        }

        if (ctx->options.verbose) {
            printf("Built cache: %d files, scanned\n", n_files);
        }

        library_free_file_list(files, n_files);
    }

    if (cwd) free(cwd);
    return true;
}

// Look up and load a label's content
char *library_lookup_label(LibraryContext *ctx, const char *label, const char **source_file,
                           char **before_each_out, char **after_each_out, char *error_msg,
                           int error_size) {
    // Look up in index
    const char *file_path = NULL;
    int line = 0;

    if (!label_index_lookup(ctx->cache.index, label, &file_path, &line)) {
        if (error_msg) {
            snprintf(error_msg, error_size, "Label '%s' not found in library", label);
        }
        return NULL;
    }

    if (source_file) {
        *source_file = file_path;
    }

    // Read and parse the file line by line
    FILE *f = fopen(file_path, "r");
    if (!f) {
        if (error_msg) {
            snprintf(error_msg, error_size, "Cannot read %s", file_path);
        }
        return NULL;
    }

    // First pass: find #comment_char, #before_each, #after_each directives
    char *comment_char = NULL;
    char *before_each = NULL;
    char *after_each = NULL;
    char line_buf[1024];
    bool in_before_each = false;
    bool in_after_each = false;
    size_t before_capacity = 0, before_len = 0;
    size_t after_capacity = 0, after_len = 0;

    while (fgets(line_buf, sizeof(line_buf), f)) {
        const char *line_content = line_buf;
        // Check for #comment_char anywhere (could be in a comment)
        const char *cc_pos = strstr(line_buf, "#comment_char");
        if (cc_pos && !comment_char) {
            const char *p = cc_pos + 13;
            while (*p && isspace(*p)) p++;
            if (*p == '"') {
                p++;
                const char *end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    comment_char = malloc(len + 1);
                    memcpy(comment_char, p, len);
                    comment_char[len] = '\0';
                }
            }
        }

        // Strip comment char if we have one
        if (comment_char) {
            size_t cc_len = strlen(comment_char);
            if (strncmp(line_content, comment_char, cc_len) == 0) {
                line_content += cc_len;
                if (*line_content == ' ') line_content++;
            }
        }

        // Trim to check for directives
        char trimmed[1024];
        strncpy(trimmed, line_content, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        str_trim(trimmed);

        // Check for #before_each start
        if (strncmp(trimmed, "#before_each", 12) == 0) {
            in_before_each = true;
            in_after_each = false;
            continue;
        }

        // Check for #after_each start
        if (strncmp(trimmed, "#after_each", 11) == 0) {
            in_after_each = true;
            in_before_each = false;
            continue;
        }

        // Check for #end
        if (strncmp(trimmed, "#end", 4) == 0) {
            in_before_each = false;
            in_after_each = false;
            continue;
        }

        // Accumulate content for before_each/after_each
        if (in_before_each) {
            size_t line_len = strlen(line_content);
            if (before_len + line_len + 1 > before_capacity) {
                before_capacity = (before_capacity == 0) ? 1024 : (before_capacity * 2);
                before_each = realloc(before_each, before_capacity);
            }
            memcpy(before_each + before_len, line_content, line_len);
            before_len += line_len;
            if (before_each) before_each[before_len] = '\0';
        } else if (in_after_each) {
            size_t line_len = strlen(line_content);
            if (after_len + line_len + 1 > after_capacity) {
                after_capacity = (after_capacity == 0) ? 1024 : (after_capacity * 2);
                after_each = realloc(after_each, after_capacity);
            }
            memcpy(after_each + after_len, line_content, line_len);
            after_len += line_len;
            if (after_each) after_each[after_len] = '\0';
        }
    }

    if (!comment_char) {
        comment_char = str_dup("");
    }
    size_t comment_len = strlen(comment_char);

    // Return before_each/after_each if requested
    if (before_each_out) {
        *before_each_out = before_each ? before_each : str_dup("");
    } else if (before_each) {
        free(before_each);
    }

    if (after_each_out) {
        *after_each_out = after_each ? after_each : str_dup("");
    } else if (after_each) {
        free(after_each);
    }

    // Second pass: find and extract the label's content
    rewind(f);
    char *content = NULL;
    size_t content_len = 0;
    size_t content_capacity = 0;
    bool in_label = false;
    char target_label[256];
    snprintf(target_label, sizeof(target_label), "@label %s", label);

    while (fgets(line_buf, sizeof(line_buf), f)) {
        // Don't trim - we need to preserve structure
        // But check if it starts with comment_char
        const char *line_content = line_buf;

        // Skip comment char if present
        if (comment_len > 0) {
            if (strncmp(line_content, comment_char, comment_len) == 0) {
                line_content += comment_len;
                // Skip one space after comment char if present
                if (*line_content == ' ') line_content++;
            } else if (in_label) {
                // In label but line doesn't start with comment char - stop
                break;
            } else {
                // Not in label, skip this line
                continue;
            }
        }

        // Check for @label or @end
        char line_trimmed[1024];
        strncpy(line_trimmed, line_content, sizeof(line_trimmed) - 1);
        line_trimmed[sizeof(line_trimmed) - 1] = '\0';
        str_trim(line_trimmed);

        if (strncmp(line_trimmed, target_label, strlen(target_label)) == 0) {
            in_label = true;
            continue; // Don't include @label line
        }

        if (in_label) {
            if (strncmp(line_trimmed, "@end", 4) == 0) {
                break; // Found end
            }
            // Add this line to content (already stripped of comment char)
            size_t line_len = strlen(line_content);
            if (content_len + line_len + 1 > content_capacity) {
                content_capacity = (content_capacity == 0) ? 1024 : (content_capacity * 2);
                content = realloc(content, content_capacity);
            }
            memcpy(content + content_len, line_content, line_len);
            content_len += line_len;
            content[content_len] = '\0';
        }
    }

    fclose(f);
    free(comment_char);

    if (!content && error_msg) {
        snprintf(error_msg, error_size, "Cannot parse label '%s' from %s", label, file_path);
    }

    return content;
}

// Get suggestions for misspelled labels
char **library_suggest_labels(const LibraryContext *ctx, const char *label, int *n_suggestions) {
    char **all_labels;
    int n_all;
    label_index_get_all(ctx->cache.index, &all_labels, &n_all);

    // Simple suggestion: find labels that start with same prefix or contain substring
    char **suggestions = NULL;
    *n_suggestions = 0;
    int capacity = 0;

    for (int i = 0; i < n_all && *n_suggestions < 5; i++) {
        if (strstr(all_labels[i], label) || strstr(label, all_labels[i])) {
            if (*n_suggestions >= capacity) {
                capacity = (capacity == 0) ? 5 : (capacity * 2);
                suggestions = realloc(suggestions, capacity * sizeof(char *));
            }
            suggestions[(*n_suggestions)++] = str_dup(all_labels[i]);
        }
    }

    // Free all_labels
    for (int i = 0; i < n_all; i++) {
        free(all_labels[i]);
    }
    free(all_labels);

    return suggestions;
}

void library_free_suggestions(char **suggestions, int n_suggestions) {
    for (int i = 0; i < n_suggestions; i++) {
        free(suggestions[i]);
    }
    free(suggestions);
}

// Look up target_prefix for a file extension
const char *library_lookup_target_prefix(const LibraryConfig *config, const char *filename) {
    if (!config || !filename) return NULL;

    // Find the file extension
    const char *ext = strrchr(filename, '.');
    if (!ext) return NULL;

    // Look up in the target_prefix_map
    for (int i = 0; i < config->n_target_prefix; i++) {
        if (strcmp(config->target_prefix_map[i].extension, ext) == 0) {
            return config->target_prefix_map[i].value;
        }
    }

    return NULL;
}
