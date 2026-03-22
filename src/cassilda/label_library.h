// label_library.h - Label library discovery and caching for Cassilda
//
// This module implements a flexible label discovery system that allows
// Cassilda to find label definitions in external .cld files using:
//
// - Config files (.cassilda.json) to specify library locations
// - Automatic directory scanning for .cld files
// - Performance caching to avoid re-parsing unchanged files
// - Safe upward directory search (opt-in with --find-config)
//
// Search priority order:
// 1. Labels defined in the processed file itself
// 2. Cached labels from configured libraries
// 3. Fallback: .cld files in processed file's directory
// 4. Fallback: .cld files in current working directory

#ifndef LABEL_LIBRARY_H
#define LABEL_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// ============================================================================
// Configuration (.cassilda.json)
// ============================================================================

// Extension→prefix mapping for target files
typedef struct {
    char *extension; // e.g., ".c", ".h", ".py"
    char *value;     // e.g., " * ", "# ", etc.
} ExtensionMapping;

typedef struct {
    char **library_files; // Explicit paths to .cld files
    int n_library_files;

    char **library_dirs; // Directories containing .cld files
    int n_library_dirs;

    bool include_subdirs; // Recursively search directories
    char *cache_path;     // Path to cache file (default: .cassilda-cache)

    // Extension-based target prefix from .cassilda.json
    ExtensionMapping *target_prefix_map; // Array of extension→prefix mappings
    int n_target_prefix;
} LibraryConfig;

// Initialize config with defaults
void library_config_init(LibraryConfig *config);

// Free config resources
void library_config_free(LibraryConfig *config);

// Parse config from JSON file
// Returns true on success, false on error (sets error_msg)
bool library_config_parse(LibraryConfig *config, const char *json_path, char *error_msg,
                          int error_size);

// ============================================================================
// Cache (.cassilda-cache)
// ============================================================================

// Metadata for a tracked .cld file
typedef struct {
    char *path;    // Absolute path to .cld file
    time_t mtime;  // Last modification time
    size_t size;   // File size in bytes
    char **labels; // Labels defined in this file
    int n_labels;  // Number of labels
} TrackedFile;

// Cache structure
typedef struct {
    char *config_hash;        // SHA256 of config (to detect changes)
    TrackedFile *files;       // Tracked .cld files
    int n_files;              // Number of tracked files
    int files_capacity;       // Capacity for tracked files array
    time_t generated;         // When cache was generated
    struct LabelIndex *index; // Fast label→file lookup
} LabelCache;

// Initialize empty cache
void label_cache_init(LabelCache *cache);

// Free cache resources
void label_cache_free(LabelCache *cache);

// Load cache from file
// Returns true if cache exists and is valid, false otherwise
bool label_cache_load(LabelCache *cache, const char *cache_path, char *error_msg, int error_size);

// Save cache to file
bool label_cache_save(const LabelCache *cache, const char *cache_path, char *error_msg,
                      int error_size);

// Check if cache is still valid (no file changes)
// Returns true if cache is valid, false if rebuild needed
bool label_cache_is_valid(const LabelCache *cache, const LibraryConfig *config);

// ============================================================================
// Label Index (fast lookup)
// ============================================================================

typedef struct LabelIndex LabelIndex;

// Create a new label index
LabelIndex *label_index_create(void);

// Free label index
void label_index_free(LabelIndex *index);

// Add label→file mapping
void label_index_add(LabelIndex *index, const char *label, const char *file_path, int line);

// Look up a label, returns file path and line number
// Returns true if found, false otherwise
bool label_index_lookup(const LabelIndex *index, const char *label, const char **file_path,
                        int *line);

// Get all labels (for error suggestions)
void label_index_get_all(const LabelIndex *index, char ***labels, int *n_labels);

// ============================================================================
// Library Discovery
// ============================================================================

// Options for library discovery
typedef struct {
    const char *processed_file; // File being processed (for relative paths)
    const char *config_path;    // Explicit config path (or NULL)
    bool find_config;           // Search parent directories for config
    bool rebuild_cache;         // Force cache rebuild
    bool no_cache;              // Disable caching
    bool verbose;               // Print discovery information
} LibraryOptions;

// Initialize options with defaults
void library_options_init(LibraryOptions *opts);

// Main library discovery context
typedef struct {
    LibraryConfig config;   // Loaded configuration
    LabelCache cache;       // Label cache
    LibraryOptions options; // Discovery options
    bool has_config;        // Whether config was found
    char *config_path;      // Resolved config path (if found)
    char *cache_path;       // Resolved cache path
} LibraryContext;

// Initialize library context
void library_context_init(LibraryContext *ctx, const LibraryOptions *opts);

// Free library context
void library_context_free(LibraryContext *ctx);

// Discover and load library (config + cache)
// Returns true on success, false on error
bool library_discover(LibraryContext *ctx, char *error_msg, int error_size);

// Look up a label in the library
// Returns newly allocated content string, or NULL if not found
// If found, sets *source_file to the file path (do not free)
// If before_each_out/after_each_out are non-NULL, returns the before_each/after_each from that file
char *library_lookup_label(LibraryContext *ctx, const char *label, const char **source_file,
                           char **before_each_out, char **after_each_out, char *error_msg,
                           int error_size);

// Get suggestions for a misspelled label (for error messages)
// Returns newly allocated array of strings (free with free_suggestions)
char **library_suggest_labels(const LibraryContext *ctx, const char *label, int *n_suggestions);
void library_free_suggestions(char **suggestions, int n_suggestions);

// Look up target_prefix for a file extension
// Returns the value from the map, or NULL if not found
// Caller should NOT free the returned string
const char *library_lookup_target_prefix(const LibraryConfig *config, const char *filename);

// ============================================================================
// Utilities
// ============================================================================

// Find config file, optionally searching parent directories
// Returns newly allocated path, or NULL if not found
char *library_find_config(const char *start_dir, bool search_up);

// Scan directory for .cld files
// Returns newly allocated array of paths (free with free_file_list)
char **library_scan_directory(const char *dir, bool recursive, int *n_files, char *error_msg,
                              int error_size);
void library_free_file_list(char **files, int n_files);

// Get directory of a file path
// Returns newly allocated string
char *library_get_directory(const char *file_path);

// Compute SHA256 hash of a string
// Returns newly allocated hex string
char *library_compute_hash(const char *data);

// Check if file can be read (safe permission check)
bool library_can_read_file(const char *path);

// Get file modification time and size
bool library_get_file_info(const char *path, time_t *mtime, size_t *size);

#endif // LABEL_LIBRARY_H
