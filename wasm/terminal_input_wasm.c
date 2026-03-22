// terminal_input_wasm.c — Stub implementation for WASM builds
// Terminal input is not available in the browser/LSP environment.

#include "utils/terminal_input.h"
#include <stdbool.h>
#include <stddef.h>

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
    return 24;
}

int terminal_get_cols(void) {
    return 80;
}
