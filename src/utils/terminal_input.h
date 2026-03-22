#ifndef TERMINAL_INPUT_H
#define TERMINAL_INPUT_H

#include <stdbool.h>

// Initialize terminal for raw input (keyboard + mouse tracking)
// Returns true on success, false on failure
bool terminal_input_init(void);

// Restore terminal to original state
void terminal_input_cleanup(void);

// Poll for input events (non-blocking)
// Call this regularly in the main loop
void terminal_input_poll(void);

// Keyboard functions
bool terminal_has_key(void);
const char *terminal_get_key(void); // Returns key name, consumes it

// Mouse functions
int terminal_get_mouse_x(void);      // 0-based column
int terminal_get_mouse_y(void);      // 0-based row
int terminal_get_mouse_button(void); // 0=none, 1=left, 2=middle, 3=right

// Check if terminal input is active
bool terminal_input_is_active(void);

// Time functions
int terminal_get_time_ms(void); // Milliseconds since init (for frame timing)

// Terminal size functions (work without init, query current terminal)
int terminal_get_rows(void); // Number of rows in terminal
int terminal_get_cols(void); // Number of columns in terminal

#endif // TERMINAL_INPUT_H
