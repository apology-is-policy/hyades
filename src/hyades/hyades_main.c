// hyades_main.c - Simple Hyades typesetting CLI
// Takes Hyades/LaTeX source, outputs rendered Unicode/ASCII text

#include "compositor/compositor.h"
#include "document/document.h"
#include "math/renderer/symbols.h"
#include "public_api/hyades_api.h"
#include "utils/error.h"
#include "utils/terminal_input.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define STDOUT_FILENO 1
#define write(fd, buf, len) _write(fd, buf, (unsigned int)(len))
#else
#include <unistd.h>
#endif

// Global flag for continuous mode exit (set by signal handler)
static volatile sig_atomic_t g_exit_requested = 0;

// Signal handler for Ctrl+C (async-signal-safe)
static void sigint_handler(int sig) {
    (void)sig;
    g_exit_requested = 1;
    // Use write() which is async-signal-safe, not printf()
    // Disable mouse tracking and show cursor
    const char *cleanup = "\033[?1003l\033[?1006l\033[?25h\n";
    write(STDOUT_FILENO, cleanup, strlen(cleanup));
}

// ============================================================================
// File I/O Helpers
// ============================================================================

// Strip carriage returns in-place (CRLF → LF)
static void strip_cr(char *s) {
    char *w = s;
    for (const char *r = s; *r; r++) {
        if (*r != '\r') *w++ = *r;
    }
    *w = '\0';
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(content, 1, size, f);
    content[bytes_read] = '\0';
    fclose(f);

    strip_cr(content);
    return content;
}

static char *read_stdin(void) {
    size_t capacity = 4096;
    size_t len = 0;
    char *content = malloc(capacity);

    int c;
    while ((c = getchar()) != EOF) {
        if (len + 1 >= capacity) {
            capacity *= 2;
            content = realloc(content, capacity);
        }
        content[len++] = (char)c;
    }
    content[len] = '\0';

    strip_cr(content);
    return content;
}

// ============================================================================
// Usage
// ============================================================================

static void print_usage(const char *program) {
    fprintf(stderr,
            "Hyades - LaTeX-like Typesetting to Unicode/ASCII\n"
            "\n"
            "Usage:\n"
            "  %s [options] [file]      Render file (or stdin if no file)\n"
            "  %s --help                Show this help message\n"
            "\n"
            "Options:\n"
            "  -w, --width <n>          Set output width (default: 80)\n"
            "  -a, --ascii              Use ASCII output instead of Unicode\n"
            "  -n, --no-math-italic     Disable math italic styling\n"
            "  -c, --continuous         Continuous mode: run \\main{} in a loop\n"
            "                           Use \\wait{ms} for timing, Ctrl+C to exit\n"
            "\n"
            "Examples:\n"
            "  echo '$$\\\\frac{a}{b}$$' | %s\n"
            "  %s document.tex\n"
            "  %s -w 60 --ascii < input.txt\n"
            "  %s -c game_of_life.cld   # Run animation\n"
            "\n",
            program, program, program, program, program, program);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    // Enable ANSI escape sequence processing
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    // Initialize symbol tables (required for rendering)
    symbols_init();

    // Default options
    int width = 80;
    bool use_unicode = true;
    bool math_italic = true;
    bool continuous_mode = false;
    const char *input_file = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-w") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires a number\n", argv[i]);
                return 1;
            }
            width = atoi(argv[++i]);
            if (width < 10 || width > 1000) {
                fprintf(stderr, "Error: width must be between 10 and 1000\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--ascii") == 0 || strcmp(argv[i], "-a") == 0) {
            use_unicode = false;
        } else if (strcmp(argv[i], "--no-math-italic") == 0 || strcmp(argv[i], "-n") == 0) {
            math_italic = false;
        } else if (strcmp(argv[i], "--continuous") == 0 || strcmp(argv[i], "-c") == 0) {
            continuous_mode = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (input_file) {
                fprintf(stderr, "Error: Multiple input files not supported\n");
                return 1;
            }
            input_file = argv[i];
        }
    }

    // Read input
    char *input;
    if (input_file) {
        input = read_file(input_file);
        if (!input) {
            fprintf(stderr, "Error: Cannot read '%s'\n", input_file);
            return 1;
        }
    } else {
        input = read_stdin();
    }

    // Build settings prefix
    char settings[256];
    snprintf(settings, sizeof(settings), "\\setunicode{%s}\n\\setmathitalic{%s}\n\\setwidth{%d}\n",
             use_unicode ? "true" : "false", math_italic ? "true" : "false", width);

    // Prepend settings to input
    size_t settings_len = strlen(settings);
    size_t input_len = strlen(input);
    char *full_input = malloc(settings_len + input_len + 1);
    memcpy(full_input, settings, settings_len);
    memcpy(full_input + settings_len, input, input_len + 1);
    free(input);

    // Set up continuous mode if requested
    if (continuous_mode) {
        // Set up signal handler for graceful exit
#ifdef _WIN32
        signal(SIGINT, sigint_handler);
#else
        struct sigaction sa;
        sa.sa_handler = sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
#endif

        // Initialize terminal for raw input (keyboard + mouse)
        if (!terminal_input_init()) {
            fprintf(stderr, "Warning: Could not initialize terminal input\n");
        }

        // Move cursor home and hide cursor (don't clear - let first frame draw)
        printf("\033[H");    // Move cursor to home position
        printf("\033[?25l"); // Hide cursor
        fflush(stdout);

        // Enable continuous mode - \main{} will handle the loop
        macro_set_continuous_mode(true, &g_exit_requested);
    }

    // Render
    CompOptions opt = {0};
    ParseError err = {0};

    char *output = compose_document(full_input, &opt, &err);
    free(full_input);

    // Disable continuous mode and cleanup terminal
    if (continuous_mode) {
        macro_set_continuous_mode(false, NULL);
        terminal_input_cleanup();
        printf("\033[?25h"); // Show cursor
        fflush(stdout);
    }

    if (!output) {
        if (err.message[0]) {
            fprintf(stderr, "Error: %s\n", err.message);
            if (err.row > 0) {
                fprintf(stderr, "  at line %d, column %d\n", err.row, err.col);
            }
        } else {
            fprintf(stderr, "Error: Rendering failed\n");
        }
        return 1;
    }

    // Output result (in continuous mode, \main{} already printed frames)
    printf("%s", output);
    free(output);

    return 0;
}
