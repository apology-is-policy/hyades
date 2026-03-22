#include "terminal_input.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// Original terminal settings (for restoration)
static struct termios g_orig_termios;
static bool g_terminal_initialized = false;

// Time tracking
static struct timespec g_start_time;
static bool g_time_initialized = false;

// Input state
static char g_last_key[32] = "";
static bool g_key_available = false;
static int g_mouse_x = -1;
static int g_mouse_y = -1;
static int g_mouse_button = 0; // 0=none, 1=left, 2=middle, 3=right

// Input buffer for parsing escape sequences
static char g_input_buf[256];
static int g_input_len = 0;

bool terminal_input_is_active(void) {
    return g_terminal_initialized;
}

bool terminal_input_init(void) {
    if (g_terminal_initialized) return true;

    // Check if stdin is a terminal
    if (!isatty(STDIN_FILENO)) {
        return false;
    }

    // Save original terminal settings
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == -1) {
        return false;
    }

    // Set raw mode but KEEP signal handling (ISIG) for Ctrl+C
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN); // Disable echo, canonical mode, but keep ISIG
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); // Disable flow control, CR->NL
    // Keep OPOST enabled for proper newline handling
    raw.c_cc[VMIN] = 0;  // Non-blocking read
    raw.c_cc[VTIME] = 0; // No timeout

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return false;
    }

    // Enable mouse tracking (any-event mode + SGR extended coordinates)
    // SGR mode gives us coordinates > 223 and cleaner button reporting
    printf("\033[?1003h"); // Enable any-event tracking
    printf("\033[?1006h"); // Enable SGR extended mode
    fflush(stdout);

    // Initialize time tracking
    if (!g_time_initialized) {
        clock_gettime(CLOCK_MONOTONIC, &g_start_time);
        g_time_initialized = true;
    }

    g_terminal_initialized = true;
    return true;
}

void terminal_input_cleanup(void) {
    if (!g_terminal_initialized) return;

    // Disable mouse tracking
    printf("\033[?1006l"); // Disable SGR mode
    printf("\033[?1003l"); // Disable any-event tracking
    fflush(stdout);

    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);

    g_terminal_initialized = false;
}

// Parse a key from the input buffer
// Returns number of bytes consumed, 0 if incomplete sequence
static int parse_key(const char *buf, int len, char *out_key, int out_size) {
    if (len == 0) return 0;

    // Check for Ctrl+C (byte 3)
    if (buf[0] == 3) {
        strncpy(out_key, "CTRL_C", out_size - 1);
        out_key[out_size - 1] = '\0';
        return 1;
    }

    // Check for escape sequences
    if (buf[0] == '\033') {
        if (len < 2) return 0; // Need more data

        // Check for CSI sequences (ESC [)
        if (buf[1] == '[') {
            if (len < 3) return 0; // Need more data

            // Check for SGR mouse sequence: ESC [ < Cb ; Cx ; Cy M/m
            if (buf[2] == '<') {
                // SGR mouse format: \033[<Cb;Cx;CyM or \033[<Cb;Cx;Cym
                // Find the terminating M or m
                int i;
                for (i = 3; i < len; i++) {
                    if (buf[i] == 'M' || buf[i] == 'm') break;
                }
                if (i >= len) return 0; // Incomplete

                // Parse: <button;x;y
                int button, x, y;
                if (sscanf(buf + 3, "%d;%d;%d", &button, &x, &y) == 3) {
                    // SGR button encoding:
                    // 0 = left press, 1 = middle press, 2 = right press
                    // 32 = left drag, 33 = middle drag, 34 = right drag
                    // 35 = motion (no button)
                    // 64 = scroll up, 65 = scroll down

                    bool is_release = (buf[i] == 'm');
                    int base_button = button & 3; // Lower 2 bits
                    bool is_motion = (button & 32) != 0;

                    g_mouse_x = x - 1; // Convert to 0-based
                    g_mouse_y = y - 1;

                    if (is_release) {
                        g_mouse_button = 0;
                    } else if (!is_motion || base_button < 3) {
                        // Press or drag
                        if (base_button == 0)
                            g_mouse_button = 1; // Left
                        else if (base_button == 1)
                            g_mouse_button = 2; // Middle
                        else if (base_button == 2)
                            g_mouse_button = 3; // Right
                    }
                    // For pure motion (button == 35), keep previous button state
                    // but update position (already done above)
                }

                // This was a mouse event, not a key
                out_key[0] = '\0';
                return i + 1;
            }

            // Arrow keys and other special keys
            if (buf[2] >= 'A' && buf[2] <= 'D') {
                // Arrow keys: A=up, B=down, C=right, D=left
                const char *names[] = {"UP", "DOWN", "RIGHT", "LEFT"};
                strncpy(out_key, names[buf[2] - 'A'], out_size - 1);
                out_key[out_size - 1] = '\0';
                return 3;
            }

            // Check for longer sequences like F1-F12, Home, End, etc.
            // F1-F4: ESC O P/Q/R/S or ESC [ 1 1 ~ through ESC [ 1 4 ~
            // Home: ESC [ H or ESC [ 1 ~
            // End: ESC [ F or ESC [ 4 ~
            // etc.

            if (buf[2] == 'H') {
                strncpy(out_key, "HOME", out_size - 1);
                out_key[out_size - 1] = '\0';
                return 3;
            }
            if (buf[2] == 'F') {
                strncpy(out_key, "END", out_size - 1);
                out_key[out_size - 1] = '\0';
                return 3;
            }

            // Sequences ending with ~: ESC [ <num> ~
            if (len >= 4 && buf[3] == '~') {
                const char *names[] = {NULL, "HOME", "INSERT", "DELETE", "END", "PGUP", "PGDN"};
                int n = buf[2] - '0';
                if (n >= 1 && n <= 6 && names[n]) {
                    strncpy(out_key, names[n], out_size - 1);
                    out_key[out_size - 1] = '\0';
                }
                return 4;
            }

            // Longer sequences: ESC [ <num> <num> ~
            if (len >= 5 && buf[4] == '~') {
                int n = (buf[2] - '0') * 10 + (buf[3] - '0');
                char tmp[16];
                if (n >= 11 && n <= 15) {
                    snprintf(tmp, sizeof(tmp), "F%d", n - 10);
                } else if (n >= 17 && n <= 21) {
                    snprintf(tmp, sizeof(tmp), "F%d", n - 11);
                } else if (n >= 23 && n <= 24) {
                    snprintf(tmp, sizeof(tmp), "F%d", n - 12);
                } else {
                    snprintf(tmp, sizeof(tmp), "KEY_%d", n);
                }
                strncpy(out_key, tmp, out_size - 1);
                out_key[out_size - 1] = '\0';
                return 5;
            }
        }

        // ESC O sequences (SS3)
        if (buf[1] == 'O' && len >= 3) {
            // F1-F4: ESC O P/Q/R/S
            if (buf[2] >= 'P' && buf[2] <= 'S') {
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "F%d", buf[2] - 'P' + 1);
                strncpy(out_key, tmp, out_size - 1);
                out_key[out_size - 1] = '\0';
                return 3;
            }
        }

        // Unknown escape sequence - treat as ESC key if followed by nothing
        // or skip if we can't parse it
        if (len == 1) {
            strncpy(out_key, "ESCAPE", out_size - 1);
            out_key[out_size - 1] = '\0';
            return 1;
        }

        // Unknown sequence - skip the escape and try again
        return 1;
    }

    // Regular character
    if (buf[0] == ' ') {
        strncpy(out_key, "SPACE", out_size - 1);
    } else if (buf[0] == '\r' || buf[0] == '\n') {
        strncpy(out_key, "ENTER", out_size - 1);
    } else if (buf[0] == '\t') {
        strncpy(out_key, "TAB", out_size - 1);
    } else if (buf[0] == 127 || buf[0] == 8) {
        strncpy(out_key, "BACKSPACE", out_size - 1);
    } else if (buf[0] >= 1 && buf[0] <= 26) {
        // Ctrl+letter
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "CTRL_%c", 'A' + buf[0] - 1);
        strncpy(out_key, tmp, out_size - 1);
    } else {
        // Regular printable character
        out_key[0] = buf[0];
        out_key[1] = '\0';
    }
    out_key[out_size - 1] = '\0';
    return 1;
}

void terminal_input_poll(void) {
    if (!g_terminal_initialized) return;

    // Read any available input
    char buf[256];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

    if (n > 0) {
        // Append to input buffer
        int space = sizeof(g_input_buf) - g_input_len - 1;
        if (n > space) n = space;
        memcpy(g_input_buf + g_input_len, buf, n);
        g_input_len += n;
        g_input_buf[g_input_len] = '\0';
    }

    // Parse input buffer
    while (g_input_len > 0) {
        char key[32] = "";
        int consumed = parse_key(g_input_buf, g_input_len, key, sizeof(key));

        if (consumed == 0) {
            // Incomplete sequence, wait for more data
            break;
        }

        // Remove consumed bytes from buffer
        memmove(g_input_buf, g_input_buf + consumed, g_input_len - consumed + 1);
        g_input_len -= consumed;

        // If we got a key (not just a mouse event), store it
        if (key[0] != '\0') {
            strncpy(g_last_key, key, sizeof(g_last_key) - 1);
            g_last_key[sizeof(g_last_key) - 1] = '\0';
            g_key_available = true;

            // Handle Ctrl+C specially - request exit
            if (strcmp(key, "CTRL_C") == 0) {
                // Set the exit flag if available
                extern volatile int *macro_get_exit_flag(void);
                volatile int *exit_flag = macro_get_exit_flag();
                if (exit_flag) {
                    *exit_flag = 1;
                }
            }
        }
    }
}

bool terminal_has_key(void) {
    return g_key_available;
}

const char *terminal_get_key(void) {
    if (!g_key_available) return "";
    g_key_available = false;
    return g_last_key;
}

int terminal_get_mouse_x(void) {
    return g_mouse_x;
}

int terminal_get_mouse_y(void) {
    return g_mouse_y;
}

int terminal_get_mouse_button(void) {
    return g_mouse_button;
}

int terminal_get_time_ms(void) {
    if (!g_time_initialized) {
        // Initialize on first call even if terminal not active
        clock_gettime(CLOCK_MONOTONIC, &g_start_time);
        g_time_initialized = true;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Calculate milliseconds since start
    long sec_diff = now.tv_sec - g_start_time.tv_sec;
    long nsec_diff = now.tv_nsec - g_start_time.tv_nsec;

    return (int)(sec_diff * 1000 + nsec_diff / 1000000);
}

// Get terminal dimensions using ioctl
// These work without terminal_input_init() - they just query the terminal size
#include <sys/ioctl.h>

int terminal_get_rows(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return 24; // Default fallback
    }
    return ws.ws_row;
}

int terminal_get_cols(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return 80; // Default fallback
    }
    return ws.ws_col;
}
