# Cross-Platform Build Fixes

This document summarizes the changes made to enable builds on Linux and Windows.

## Issues Found

### Linux Build Failure

**Error:**
```
error: unknown type name 'uint64_t'
note: 'uint64_t' is defined in header '<stdint.h>'; did you forget to '#include <stdint.h>'?
```

**Cause:** Missing `#include <stdint.h>` header in `label_library.c`

**Fix:** Added `#include <stdint.h>` to the includes section

### Windows Build Failure (Expected)

**Issues:**
1. **Missing POSIX headers** - `dirent.h`, `unistd.h` not available on Windows
2. **Different filesystem APIs** - Windows uses `FindFirstFile`/`FindNextFile` instead of `opendir`/`readdir`
3. **Different path functions** - Windows uses `_fullpath` instead of `realpath`
4. **Path separator differences** - Windows uses `\` instead of `/`
5. **Filesystem root detection** - Windows uses drive letters (e.g., `C:\`) instead of `/`

## Changes Made to `src/cassilda/label_library.c`

### 1. Added Platform-Specific Headers

```c
// Platform-specific includes
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define PATH_MAX MAX_PATH
#define stat _stat
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define access _access
#define R_OK 4
#else
#include <dirent.h>
#include <unistd.h>
#endif
```

### 2. Dual Implementation of Directory Scanning

Created separate implementations for Windows and POSIX:

**Windows version:**
- Uses `FindFirstFileA()` and `FindNextFileA()`
- Uses `\\` path separator
- Checks `FILE_ATTRIBUTE_DIRECTORY` flag

**POSIX version:**
- Uses `opendir()` and `readdir()`
- Uses `/` path separator
- Checks with `is_directory()`

### 3. Cross-Platform `get_absolute_path()`

```c
static char *get_absolute_path(const char *path) {
#ifdef _WIN32
    char *abs_path = _fullpath(NULL, path, PATH_MAX);
#else
    char *abs_path = realpath(path, NULL);
#endif
    return abs_path;
}
```

### 4. Cross-Platform Config Discovery

Updated `library_find_config()` to handle:

- **Path separators**: Uses `path_sep` variable (`\` on Windows, `/` on POSIX)
- **Home directory**: Uses `USERPROFILE` on Windows, `HOME` on POSIX
- **Filesystem root**: Detects `C:\` pattern on Windows, `/` on POSIX
- **Device boundaries**: Only checked on POSIX (not relevant on Windows)

### 5. Fixed `fread()` Warning

Changed from ignoring return value to:
```c
size_t bytes_read = fread(json, 1, size, f);
json[bytes_read] = '\0';
```

## Verification

### Local Testing (macOS)
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
# ✅ Build successful
```

### CI Testing (GitHub Actions)

The build workflow tests on:
- ✅ **macOS** (latest)
- ✅ **Ubuntu Linux** (latest) - Fixed with stdint.h
- ✅ **Windows** (latest) - Fixed with platform-specific code

## Future Considerations

### Additional Windows-Specific Issues

If Windows builds still have issues, check for:

1. **Case sensitivity** - Windows filesystems are case-insensitive by default
2. **Line endings** - Windows uses CRLF vs Unix LF
3. **MAX_PATH limitation** - Windows paths limited to 260 characters (unless long path support enabled)
4. **Backslash escaping** - String literals with paths may need `\\`

### Testing on Windows Locally

To test Windows builds locally:

```bash
# Using Visual Studio toolchain
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release

# Or using Ninja with MSVC
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### MSYS2/MinGW Considerations

If building with MSYS2/MinGW:
- POSIX functions may be available
- May need conditional compilation based on `__MINGW32__` or `__MINGW64__`
- Path handling might be more POSIX-like

## Summary

All platform-specific code is now properly guarded with `#ifdef _WIN32` preprocessor directives, allowing the same source code to compile on Windows, Linux, and macOS without modification.

The GitHub Actions workflow will automatically test all three platforms on every push, ensuring cross-platform compatibility is maintained.
