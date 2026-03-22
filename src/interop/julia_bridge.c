// julia_bridge.c — Julia computation bridge for Hyades
// Dynamic loading version: finds and loads Julia at runtime if available
// Works on macOS, Linux, and Windows
//
// Strategy: Use minimal exported symbols and rely on jl_eval_string for
// type checking, since many Julia C API functions are inline/macros.

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#ifdef _WIN32
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static int hyades_vasprintf(char **out, const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);

    int len = _vscprintf(fmt, ap2);
    va_end(ap2);

    if (len < 0) {
        *out = NULL;
        return -1;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        *out = NULL;
        return -1;
    }

    int written = vsnprintf(buf, (size_t)len + 1, fmt, ap);
    if (written < 0) {
        free(buf);
        *out = NULL;
        return -1;
    }

    *out = buf;
    return written;
}

static int hyades_asprintf(char **out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = hyades_vasprintf(out, fmt, ap);
    va_end(ap);
    return r;
}

#define asprintf hyades_asprintf
#endif

#include "julia_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Platform-specific dynamic loading
// ============================================================================

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define DLOPEN(path) LoadLibraryA(path)
#define DLSYM(handle, sym) GetProcAddress((HMODULE)(handle), sym)
#define DLCLOSE(handle) FreeLibrary((HMODULE)(handle))
#define PATH_SEP "\\"
#define JULIA_LIB_NAME "libjulia.dll"
typedef HMODULE lib_handle_t;
#else
#include <dlfcn.h>
#include <glob.h>
#include <unistd.h>
#define DLOPEN(path) dlopen(path, RTLD_LAZY | RTLD_GLOBAL)
#define DLSYM(handle, sym) dlsym(handle, sym)
#define DLCLOSE(handle) dlclose(handle)
#define PATH_SEP "/"
#ifdef __APPLE__
#define JULIA_LIB_NAME "libjulia.dylib"
#else
#define JULIA_LIB_NAME "libjulia.so"
#endif
typedef void *lib_handle_t;
#endif

// ============================================================================
// Julia types (opaque)
// ============================================================================

typedef struct _jl_value_t jl_value_t;
typedef jl_value_t jl_function_t;
typedef struct _jl_module_t jl_module_t;
typedef struct _jl_sym_t jl_sym_t;

// ============================================================================
// Julia function pointer types - ONLY truly exported symbols
// ============================================================================

typedef void (*jl_init_fn)(void);
typedef void (*jl_atexit_hook_fn)(int);
typedef jl_value_t *(*jl_eval_string_fn)(const char *);
typedef jl_value_t *(*jl_exception_occurred_fn)(void);
typedef void (*jl_exception_clear_fn)(void);
typedef jl_value_t *(*jl_get_global_fn)(jl_module_t *, jl_sym_t *);
typedef jl_sym_t *(*jl_symbol_fn)(const char *);
typedef jl_value_t *(*jl_call1_fn)(jl_function_t *, jl_value_t *);
typedef jl_value_t *(*jl_call2_fn)(jl_function_t *, jl_value_t *, jl_value_t *);
typedef const char *(*jl_string_ptr_fn)(jl_value_t *);
typedef const char *(*jl_typeof_str_fn)(jl_value_t *);
typedef int64_t (*jl_unbox_int64_fn)(jl_value_t *);
typedef double (*jl_unbox_float64_fn)(jl_value_t *);

// ============================================================================
// Global state
// ============================================================================

static lib_handle_t g_julia_lib = NULL;
static bool g_initialized = false;

// Function pointers
static jl_init_fn p_jl_init = NULL;
static jl_atexit_hook_fn p_jl_atexit_hook = NULL;
static jl_eval_string_fn p_jl_eval_string = NULL;
static jl_exception_occurred_fn p_jl_exception_occurred = NULL;
static jl_exception_clear_fn p_jl_exception_clear = NULL;
static jl_get_global_fn p_jl_get_global = NULL;
static jl_symbol_fn p_jl_symbol = NULL;
static jl_call1_fn p_jl_call1 = NULL;
static jl_call2_fn p_jl_call2 = NULL;
static jl_string_ptr_fn p_jl_string_ptr = NULL;
static jl_typeof_str_fn p_jl_typeof_str = NULL;
static jl_unbox_int64_fn p_jl_unbox_int64 = NULL;
static jl_unbox_float64_fn p_jl_unbox_float64 = NULL;

// Global module pointer
static jl_module_t *p_jl_base_module = NULL;

// ============================================================================
// Configuration
// ============================================================================

static int g_float_precision = 6;
static bool g_scientific_notation = true;

void julia_set_float_precision(int digits) {
    if (digits >= 1 && digits <= 17) g_float_precision = digits;
}

void julia_set_scientific_notation(bool enabled) {
    g_scientific_notation = enabled;
}

// ============================================================================
// Registry
// ============================================================================

typedef struct {
    char *name;
    char *params;
    char *code;
} JuliaComputation;

static JuliaComputation *g_computations = NULL;
static int g_n_computations = 0;
static int g_computations_cap = 0;

// ============================================================================
// Helpers
// ============================================================================

static JuliaResult *make_error(const char *msg) {
    JuliaResult *r = calloc(1, sizeof(JuliaResult));
    r->type = JULIA_RESULT_ERROR;
    r->string_val = strdup(msg);
    return r;
}

static JuliaResult *make_nil(void) {
    JuliaResult *r = calloc(1, sizeof(JuliaResult));
    r->type = JULIA_RESULT_NIL;
    return r;
}

// ============================================================================
// Symbol loading - minimal set of truly exported symbols
// ============================================================================

static bool load_julia_symbols(void) {
#define LOAD_SYM(name)                                                                             \
    p_##name = (name##_fn)DLSYM(g_julia_lib, #name);                                               \
    if (!p_##name) {                                                                               \
        fprintf(stderr, "Hyades: Failed to load Julia symbol '%s'\n", #name);                      \
        return false;                                                                              \
    }

    LOAD_SYM(jl_init)
    LOAD_SYM(jl_atexit_hook)
    LOAD_SYM(jl_eval_string)
    LOAD_SYM(jl_exception_occurred)
    LOAD_SYM(jl_exception_clear)
    LOAD_SYM(jl_get_global)
    LOAD_SYM(jl_symbol)
    LOAD_SYM(jl_call1)
    LOAD_SYM(jl_call2)
    LOAD_SYM(jl_string_ptr)
    LOAD_SYM(jl_typeof_str)
    LOAD_SYM(jl_unbox_int64)
    LOAD_SYM(jl_unbox_float64)

#undef LOAD_SYM

    // Load jl_base_module global
    void *ptr = DLSYM(g_julia_lib, "jl_base_module");
    if (ptr) p_jl_base_module = *(jl_module_t **)ptr;

    return true;
}

// ============================================================================
// Julia discovery
// ============================================================================

static lib_handle_t try_load_path(const char *path) {
    lib_handle_t h = DLOPEN(path);
    if (h) fprintf(stderr, "Hyades: Found Julia at %s\n", path);
    return h;
}

#ifndef _WIN32
static lib_handle_t try_glob_pattern(const char *pattern) {
    glob_t gl;
    lib_handle_t h = NULL;
    if (glob(pattern, GLOB_NOSORT, NULL, &gl) == 0) {
        for (size_t i = gl.gl_pathc; i > 0 && !h; i--) h = try_load_path(gl.gl_pathv[i - 1]);
        globfree(&gl);
    }
    return h;
}
#endif

static lib_handle_t try_julia_config(void) {
    FILE *fp;
    char cmd[512], path[4096];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
             "julia -e \"print(joinpath(Sys.BINDIR, \\\"..\\\", \\\"bin\\\", \\\"%s\\\"))\"",
             JULIA_LIB_NAME);
#else
    snprintf(cmd, sizeof(cmd), "julia -e 'print(joinpath(Sys.BINDIR, \"..\", \"lib\", \"%s\"))'",
             JULIA_LIB_NAME);
#endif
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(path, sizeof(path), fp)) {
            size_t len = strlen(path);
            if (len > 0 && path[len - 1] == '\n') path[len - 1] = '\0';
            pclose(fp);
            return try_load_path(path);
        }
        pclose(fp);
    }
    return NULL;
}

static lib_handle_t find_julia_library(void) {
    lib_handle_t h = NULL;
    char path[4096];
    const char *home = getenv("HOME");
#ifdef _WIN32
    const char *userprofile = getenv("USERPROFILE");
    const char *localappdata = getenv("LOCALAPPDATA");
#endif

    h = DLOPEN(JULIA_LIB_NAME);
    if (h) {
        fprintf(stderr, "Hyades: Found Julia in library path\n");
        return h;
    }

    const char *julia_bindir = getenv("JULIA_BINDIR");
    if (julia_bindir) {
        snprintf(path, sizeof(path), "%s" PATH_SEP ".." PATH_SEP "lib" PATH_SEP "%s", julia_bindir,
                 JULIA_LIB_NAME);
        h = try_load_path(path);
        if (h) return h;
    }

    h = try_julia_config();
    if (h) return h;

#ifdef __APPLE__
    h = try_load_path("/opt/homebrew/lib/" JULIA_LIB_NAME);
    if (h) return h;
    h = try_load_path("/usr/local/lib/" JULIA_LIB_NAME);
    if (h) return h;
    if (home) {
        snprintf(path, sizeof(path), "%s/.julia/juliaup/julia-*/lib/%s", home, JULIA_LIB_NAME);
        h = try_glob_pattern(path);
        if (h) return h;
    }
#elif defined(_WIN32)
    if (userprofile) {
        snprintf(path, sizeof(path), "%s\\.julia\\juliaup\\julia-1.12\\bin\\%s", userprofile,
                 JULIA_LIB_NAME);
        h = try_load_path(path);
        if (h) return h;
    }
    if (localappdata) {
        snprintf(path, sizeof(path), "%s\\Programs\\Julia\\bin\\%s", localappdata, JULIA_LIB_NAME);
        h = try_load_path(path);
        if (h) return h;
    }
    h = try_load_path("C:\\Julia\\bin\\" JULIA_LIB_NAME);
    if (h) return h;
#else
    if (home) {
        snprintf(path, sizeof(path), "%s/.julia/juliaup/julia-*/lib/%s", home, JULIA_LIB_NAME);
        h = try_glob_pattern(path);
        if (h) return h;
    }
    h = try_load_path("/usr/lib/" JULIA_LIB_NAME);
    if (h) return h;
    h = try_load_path("/usr/local/lib/" JULIA_LIB_NAME);
    if (h) return h;
#endif
    return NULL;
}

// ============================================================================
// Helper: get function from module (jl_get_function is inline)
// ============================================================================

static jl_function_t *get_function(jl_module_t *m, const char *name) {
    return (jl_function_t *)p_jl_get_global(m, p_jl_symbol(name));
}

// ============================================================================
// Public API: Lifecycle
// ============================================================================

bool julia_init(void) {
    if (g_initialized) return true;

    g_julia_lib = find_julia_library();
    if (!g_julia_lib) {
        fprintf(stderr, "Hyades: Julia not found. Computation features disabled.\n");
        fprintf(stderr, "Hyades: Install Julia from https://julialang.org or via juliaup\n");
        return false;
    }

    if (!load_julia_symbols()) {
        fprintf(stderr, "Hyades: Failed to load Julia symbols\n");
        DLCLOSE(g_julia_lib);
        g_julia_lib = NULL;
        return false;
    }

    p_jl_init();
    if (p_jl_exception_occurred()) {
        fprintf(stderr, "Hyades: Failed to initialize Julia runtime\n");
        p_jl_exception_clear();
        DLCLOSE(g_julia_lib);
        g_julia_lib = NULL;
        return false;
    }

    // Create HyadesCompute module and helper functions
    p_jl_eval_string(
        "module HyadesCompute\n"
        "    # Convert result to TeX string\n"
        "    function _to_tex(x)\n"
        "        if x === nothing\n"
        "            return \"\"\n"
        "        elseif x isa Integer\n"
        "            return string(x)\n"
        "        elseif x isa AbstractFloat\n"
        "            # Format nicely\n"
        "            if x == floor(x) && abs(x) < 1e15\n"
        "                return string(Int(x))\n"
        "            else\n"
        "                return string(x)\n"
        "            end\n"
        "        elseif x isa AbstractString\n"
        "            return x\n"
        "        elseif x isa AbstractVector\n"
        "            elements = [_to_tex(e) for e in x]\n"
        "            return \"\\\\pmatrix{\" * join(elements, \" \\\\\\\\ \") * \"}\"\n"
        "        elseif x isa AbstractMatrix\n"
        "            rows, cols = size(x)\n"
        "            row_strs = String[]\n"
        "            for r in 1:rows\n"
        "                row_elements = [_to_tex(x[r,c]) for c in 1:cols]\n"
        "                push!(row_strs, join(row_elements, \" & \"))\n"
        "            end\n"
        "            return \"\\\\pmatrix{\" * join(row_strs, \" \\\\\\\\ \") * \"}\"\n"
        "        else\n"
        "            return string(x)\n"
        "        end\n"
        "    end\n"
        "end\n");

    if (p_jl_exception_occurred()) {
        fprintf(stderr, "Hyades: Failed to create HyadesCompute module\n");
        p_jl_exception_clear();
        p_jl_atexit_hook(0);
        DLCLOSE(g_julia_lib);
        g_julia_lib = NULL;
        return false;
    }

    g_initialized = true;
    fprintf(stderr, "Hyades: Julia initialized successfully\n");
    return true;
}

void julia_shutdown(void) {
    if (!g_initialized) return;
    julia_clear_registry();
    if (p_jl_atexit_hook) p_jl_atexit_hook(0);
    if (g_julia_lib) {
        DLCLOSE(g_julia_lib);
        g_julia_lib = NULL;
    }
    g_initialized = false;
}

bool julia_available(void) {
    return g_initialized;
}

// ============================================================================
// Registry
// ============================================================================

static JuliaComputation *find_computation(const char *name) {
    for (int i = 0; i < g_n_computations; i++)
        if (strcmp(g_computations[i].name, name) == 0) return &g_computations[i];
    return NULL;
}

bool julia_register(const char *name, const char *params, const char *code) {
    if (!g_initialized) {
        fprintf(stderr, "Hyades: Julia not initialized\n");
        return false;
    }
    if (find_computation(name)) {
        fprintf(stderr, "Hyades: Computation '%s' already registered\n", name);
        return false;
    }

    if (g_n_computations >= g_computations_cap) {
        g_computations_cap = g_computations_cap ? g_computations_cap * 2 : 8;
        g_computations = realloc(g_computations, g_computations_cap * sizeof(JuliaComputation));
        if (!g_computations) {
            fprintf(stderr, "Hyades: Out of memory\n");
            return false;
        }
    }

    char *func_def = NULL;
    int len = (params && params[0])
                  ? asprintf(&func_def, "@eval HyadesCompute function %s(%s)\n%s\nend", name,
                             params, code)
                  : asprintf(&func_def, "@eval HyadesCompute function %s()\n%s\nend", name, code);

    if (len < 0 || !func_def) {
        fprintf(stderr, "Hyades: Out of memory\n");
        return false;
    }

    p_jl_eval_string(func_def);
    if (p_jl_exception_occurred()) {
        fprintf(stderr, "Hyades: Julia compilation error for '%s'\n", name);
        fprintf(stderr, "Hyades: Code:\n%s\n", func_def);

        // Try to show error
        if (p_jl_base_module) {
            jl_value_t *exc = p_jl_exception_occurred();
            jl_function_t *showerror = get_function(p_jl_base_module, "showerror");
            jl_function_t *stderr_f = get_function(p_jl_base_module, "stderr");
            if (showerror && stderr_f) {
                jl_value_t *io = p_jl_eval_string("stderr");
                if (io) p_jl_call2(showerror, io, exc);
            }
            fprintf(stderr, "\n");
        }

        p_jl_exception_clear();
        free(func_def);
        return false;
    }
    free(func_def);

    JuliaComputation *comp = &g_computations[g_n_computations++];
    comp->name = strdup(name);
    comp->params = params ? strdup(params) : NULL;
    comp->code = strdup(code);
    return true;
}

bool julia_is_registered(const char *name) {
    return find_computation(name) != NULL;
}

bool julia_unregister(const char *name) {
    for (int i = 0; i < g_n_computations; i++) {
        if (strcmp(g_computations[i].name, name) == 0) {
            free(g_computations[i].name);
            free(g_computations[i].params);
            free(g_computations[i].code);
            for (int j = i; j < g_n_computations - 1; j++)
                g_computations[j] = g_computations[j + 1];
            g_n_computations--;
            return true;
        }
    }
    return false;
}

void julia_clear_registry(void) {
    for (int i = 0; i < g_n_computations; i++) {
        free(g_computations[i].name);
        free(g_computations[i].params);
        free(g_computations[i].code);
    }
    free(g_computations);
    g_computations = NULL;
    g_n_computations = 0;
    g_computations_cap = 0;
}

// ============================================================================
// Execution - use Julia's _serialize helper for type detection
// ============================================================================

JuliaResult *julia_call(const char *name, const char *args) {
    if (!g_initialized) return make_error("Julia not initialized");
    if (!find_computation(name)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Unknown computation: %s", name);
        return make_error(msg);
    }

    // Call the computation and convert result to TeX via _to_tex
    char *call_str = NULL;
    int len = (args && args[0])
                  ? asprintf(&call_str, "HyadesCompute._to_tex(HyadesCompute.%s(%s))", name, args)
                  : asprintf(&call_str, "HyadesCompute._to_tex(HyadesCompute.%s())", name);
    if (len < 0 || !call_str) return make_error("Out of memory");

    jl_value_t *result = p_jl_eval_string(call_str);
    free(call_str);

    if (p_jl_exception_occurred()) {
        const char *etype = p_jl_typeof_str(p_jl_exception_occurred());
        p_jl_exception_clear();
        return make_error(etype ? etype : "Julia error");
    }

    JuliaResult *r = calloc(1, sizeof(JuliaResult));
    r->type = JULIA_RESULT_STRING;

    if (result) {
        const char *s = p_jl_string_ptr(result);
        r->string_val = s ? strdup(s) : strdup("");
    } else {
        r->string_val = strdup("");
    }

    return r;
}

JuliaResult *julia_eval(const char *code) {
    if (!g_initialized) return make_error("Julia not initialized");

    // Execute code inside HyadesCompute module, then convert result to TeX
    // This ensures `using` statements affect the same scope as registered functions
    char *wrapped = NULL;
    asprintf(&wrapped, "HyadesCompute._to_tex(Base.@eval HyadesCompute begin\n%s\nend)", code);

    if (!wrapped) return make_error("Out of memory");

    jl_value_t *result = p_jl_eval_string(wrapped);
    free(wrapped);

    if (p_jl_exception_occurred()) {
        const char *etype = p_jl_typeof_str(p_jl_exception_occurred());
        p_jl_exception_clear();
        return make_error(etype ? etype : "Julia error");
    }

    JuliaResult *r = calloc(1, sizeof(JuliaResult));
    r->type = JULIA_RESULT_STRING;

    if (result) {
        const char *s = p_jl_string_ptr(result);
        r->string_val = s ? strdup(s) : strdup("");
    } else {
        r->string_val = strdup("");
    }

    return r;
}

// ============================================================================
// Result handling
// ============================================================================

void julia_result_free(JuliaResult *result) {
    if (!result) return;
    switch (result->type) {
    case JULIA_RESULT_ERROR:
    case JULIA_RESULT_STRING:
    case JULIA_RESULT_TEX: free(result->string_val); break;
    case JULIA_RESULT_VECTOR: free(result->vector.data); break;
    case JULIA_RESULT_MATRIX: free(result->matrix.data); break;
    default: break;
    }
    free(result);
}

const char *julia_result_type_name(JuliaResultType type) {
    switch (type) {
    case JULIA_RESULT_ERROR: return "error";
    case JULIA_RESULT_NIL: return "nil";
    case JULIA_RESULT_INT: return "int";
    case JULIA_RESULT_FLOAT: return "float";
    case JULIA_RESULT_STRING: return "string";
    case JULIA_RESULT_VECTOR: return "vector";
    case JULIA_RESULT_MATRIX: return "matrix";
    case JULIA_RESULT_TEX: return "tex";
    default: return "unknown";
    }
}

static int format_float(char *buf, size_t bufsize, double val) {
    if (val == (int64_t)val && val >= -1e15 && val <= 1e15)
        return snprintf(buf, bufsize, "%.0f", val);
    if (g_scientific_notation && (val != 0) &&
        (val > 1e10 || val < -1e10 || (val < 1e-4 && val > -1e-4)))
        return snprintf(buf, bufsize, "%.*e", g_float_precision, val);
    return snprintf(buf, bufsize, "%.*g", g_float_precision, val);
}

char *julia_result_to_tex(const JuliaResult *result) {
    if (!result) return strdup("");

    char *tex = NULL;
    char buf[64];

    switch (result->type) {
    case JULIA_RESULT_NIL: tex = strdup(""); break;
    case JULIA_RESULT_ERROR:
        asprintf(&tex, "\\text{Error: %s}", result->string_val ? result->string_val : "unknown");
        break;
    case JULIA_RESULT_INT: asprintf(&tex, "%lld", (long long)result->int_val); break;
    case JULIA_RESULT_FLOAT:
        format_float(buf, sizeof(buf), result->float_val);
        tex = strdup(buf);
        break;
    case JULIA_RESULT_STRING:
    case JULIA_RESULT_TEX: tex = strdup(result->string_val ? result->string_val : ""); break;
    case JULIA_RESULT_VECTOR: {
        size_t cap = 64 + result->vector.len * 32;
        tex = malloc(cap);
        if (!tex) return strdup("\\text{OOM}");
        strcpy(tex, "\\pmatrix{");
        size_t pos = 9;
        for (int i = 0; i < result->vector.len; i++) {
            if (i > 0) {
                memcpy(tex + pos, " \\\\ ", 4);
                pos += 4;
            }
            format_float(buf, sizeof(buf), result->vector.data[i]);
            size_t blen = strlen(buf);
            if (pos + blen + 10 >= cap) {
                cap *= 2;
                tex = realloc(tex, cap);
                if (!tex) return strdup("\\text{OOM}");
            }
            memcpy(tex + pos, buf, blen);
            pos += blen;
        }
        tex[pos++] = '}';
        tex[pos] = '\0';
        break;
    }
    case JULIA_RESULT_MATRIX: {
        int rows = result->matrix.rows, cols = result->matrix.cols;
        size_t cap = 64 + rows * cols * 32;
        tex = malloc(cap);
        if (!tex) return strdup("\\text{OOM}");
        strcpy(tex, "\\pmatrix{");
        size_t pos = 9;
        for (int ro = 0; ro < rows; ro++) {
            if (ro > 0) {
                memcpy(tex + pos, " \\\\ ", 4);
                pos += 4;
            }
            for (int c = 0; c < cols; c++) {
                if (c > 0) {
                    memcpy(tex + pos, " & ", 3);
                    pos += 3;
                }
                double val = result->matrix.is_column_major ? result->matrix.data[c * rows + ro]
                                                            : result->matrix.data[ro * cols + c];
                format_float(buf, sizeof(buf), val);
                size_t blen = strlen(buf);
                if (pos + blen + 10 >= cap) {
                    cap *= 2;
                    tex = realloc(tex, cap);
                    if (!tex) return strdup("\\text{OOM}");
                }
                memcpy(tex + pos, buf, blen);
                pos += blen;
            }
        }
        tex[pos++] = '}';
        tex[pos] = '\0';
        break;
    }
    }
    return tex ? tex : strdup("");
}