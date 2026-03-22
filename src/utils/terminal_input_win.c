// terminal_input_win.c — Stub implementation for Windows builds
// Terminal raw input is not available on Windows (no termios.h).
// Interactive features (continuous mode) won't work, but rendering is unaffected.

#include "utils/terminal_input.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#endif

bool terminal_input_init(void) {
    return false;
}

void terminal_input_cleanup(void) {
}

void terminal_input_poll(void) {
}

bool terminal_has_key(void) {
    return false;
}

const char *terminal_get_key(void) {
    return NULL;
}

int terminal_get_mouse_x(void) {
    return -1;
}

int terminal_get_mouse_y(void) {
    return -1;
}

int terminal_get_mouse_button(void) {
    return 0;
}

bool terminal_input_is_active(void) {
    return false;
}

int terminal_get_time_ms(void) {
    return 0;
}

int terminal_get_rows(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#endif
    return 24;
}

int terminal_get_cols(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
#endif
    return 80;
}
