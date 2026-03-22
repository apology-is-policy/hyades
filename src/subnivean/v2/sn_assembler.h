// sn_assembler.h - Inline Subnivean Assembly
//
// Assembles human-readable mnemonics into Subnivean bytecode.
// Used by \sn{} in documents for inline assembly examples.

#ifndef SN_ASSEMBLER_H
#define SN_ASSEMBLER_H

#include "function.h"
#include "vm.h"

// Assemble source text into a Function.
// Returns NULL on error (error_msg filled).
// The returned Function has refcount=1; caller must decref when done.
//
// Syntax:
//   MNEMONIC [operand]    - instruction (operand is integer or label name)
//   label:                - defines a jump target
//   .const "string"       - add string to constant pool
//   .sym name             - intern symbol, add to constant pool
//   ; comment             - ignored
//   blank lines           - ignored
Function *sn_assemble(VM *vm, const char *source, char *error_msg, int error_size);

#endif // SN_ASSEMBLER_H
