// stdlib_lambdas.h - Standard library lambda (function) registration
//
// The implementation (stdlib_lambdas.c) is generated from stdlib/stdlib_lambdas.cld
// by the gen_stdlib_lambdas tool during the build process.
//
// These lambdas use the STD:: prefix convention (e.g., STD::SORT, STD::MAP)

#ifndef STDLIB_LAMBDAS_H
#define STDLIB_LAMBDAS_H

#include "document/calc.h"

// Register all standard library lambdas into a CalcContext's global scope.
// This function is implemented in the generated stdlib_lambdas.c file.
// Should be called once after calc_context_init().
void stdlib_register_lambdas(CalcContext *ctx);

#endif // STDLIB_LAMBDAS_H
