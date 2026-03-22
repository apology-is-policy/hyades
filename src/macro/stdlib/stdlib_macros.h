// stdlib_macros.h - Standard library macro registration
//
// The implementation (stdlib_macros.c) is generated from stdlib/stdlib.cld
// by the gen_stdlib tool during the build process.

#ifndef STDLIB_MACROS_H
#define STDLIB_MACROS_H

#include "macro/user/macro.h"

// Register all standard library macros into a registry.
// This function is implemented in the generated stdlib_macros.c file.
void stdlib_register_macros(MacroRegistry *reg);

#endif // STDLIB_MACROS_H
