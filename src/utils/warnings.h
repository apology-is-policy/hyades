// warnings.h - Non-fatal warning accumulator for render passes
#ifndef WARNINGS_H
#define WARNINGS_H

#define HYADES_MAX_WARNINGS 16
#define WARNING_MSG_SIZE 256

// Clear all accumulated warnings (call before each render)
void warnings_clear(void);

// Add a warning (printf-style). Safe to call from any compilation unit.
void hyades_add_warning(const char *fmt, ...);

// Public accessors (exposed in hyades.h)
int hyades_render_warning_count(void);
const char *hyades_render_warning_message(int index);

#endif // WARNINGS_H
