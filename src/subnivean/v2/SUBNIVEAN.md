# Subnivean 2.0 Specification

**Subnivean** is a stack-based bytecode virtual machine designed for languages with first-class scopes, mutable cells, and closure semantics. It was created to serve as a compilation target for Hyades, but its generic design allows it to support other languages with similar requirements.

The name comes from the Latin *sub* (under) + *niveus* (snowy) — referring to the hidden layer beneath the snow where small creatures live. Subnivean is the hidden computational layer beneath Hyades' document-oriented surface.

---

## Part 1: Language Specification

### 1.1 Values

Subnivean is dynamically typed. All values are tagged unions with the following types:

| Type | Description | Example |
|------|-------------|---------|
| `nil` | The absence of a value | `nil` |
| `int` | 64-bit signed integer | `42`, `-7` |
| `string` | Immutable UTF-8 string | `"hello"` |
| `symbol` | Interned identifier | `#foo` |
| `cell` | Mutable box containing a value | `Cell{42}` |
| `array` | Ordered, mutable sequence | `[1, 2, 3]` |
| `closure` | Function + captured scope | `<closure:add>` |
| `scope` | First-class environment | `<scope:5>` |

#### 1.1.1 Nil

The `nil` value represents the absence of a meaningful value. It is falsy in boolean contexts.

#### 1.1.2 Integers

Integers are 64-bit signed values. All arithmetic operations work on integers. Non-integer values are coerced to integers when used in arithmetic contexts:
- `nil` → `0`
- `string` → parsed as decimal, or `0` if invalid
- `cell` → recursively coerce contained value
- `array` → length of array

#### 1.1.3 Strings

Strings are immutable, reference-counted UTF-8 byte sequences. String operations create new strings rather than modifying existing ones.

#### 1.1.4 Symbols

Symbols are interned strings used as identifiers. Two symbols with the same name are pointer-equal, enabling O(1) comparison. Symbols are used as keys in scope bindings.

#### 1.1.5 Cells

Cells are mutable boxes that hold a single value. They enable:
- **Mutable variables**: Hyades counters (`\let<x>{0}`, `\inc<x>`)
- **Capture-by-reference**: Closures that see mutations after capture

```
Cell{42}  -- A cell containing the integer 42
```

Operations:
- `CELL_NEW`: Box a value into a new cell
- `CELL_GET`: Extract the value (copy)
- `CELL_SET`: Replace the contained value
- `CELL_INC`: Increment integer in cell, return new value
- `CELL_DEC`: Decrement integer in cell, return new value

#### 1.1.6 Arrays

Arrays are ordered, mutable sequences of values. They grow dynamically and support random access.

Operations:
- `ARRAY_NEW n`: Create array from top `n` stack values
- `ARRAY_GET`: Get element by index
- `ARRAY_SET`: Set element by index
- `ARRAY_LEN`: Get length
- `ARRAY_PUSH`: Append element
- `ARRAY_POP`: Remove and return last element

#### 1.1.7 Closures

A closure is a function paired with its captured lexical scope. When a closure is called, the captured scope becomes the parent of the new call scope.

```
Closure = Function + Captured Scope
```

This enables lexical scoping: a function always sees the variables that were in scope when it was defined, not when it was called.

#### 1.1.8 Scopes (First-Class Environments)

Scopes are first-class values that can be:
- Captured with `SCOPE_CAPTURE`
- Restored with `SCOPE_RESTORE`
- Embedded in closures

This is the key innovation enabling per-iteration scope semantics (see Section 1.4).

### 1.2 Instruction Set

Instructions are 8 bytes: a 4-byte opcode followed by a 4-byte signed operand.

#### 1.2.1 Stack Operations

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `NOP` | — | No operation |
| `POP` | `[a] → []` | Discard top of stack |
| `DUP` | `[a] → [a, a]` | Duplicate top |
| `SWAP` | `[a, b] → [b, a]` | Swap top two |
| `ROT` | `[a, b, c] → [b, c, a]` | Rotate top three |

#### 1.2.2 Constants

| Opcode | Operand | Stack Effect | Description |
|--------|---------|--------------|-------------|
| `PUSH_NIL` | — | `[] → [nil]` | Push nil |
| `PUSH_TRUE` | — | `[] → [1]` | Push integer 1 |
| `PUSH_FALSE` | — | `[] → [0]` | Push integer 0 |
| `PUSH_INT` | value | `[] → [n]` | Push immediate integer |
| `PUSH_CONST` | index | `[] → [c]` | Push from constant pool |

#### 1.2.3 Scope Operations

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `SCOPE_NEW` | — | Create child scope, enter it |
| `SCOPE_POP` | — | Exit to parent scope |
| `SCOPE_CAPTURE` | `[] → [scope]` | Push current scope as value |
| `SCOPE_RESTORE` | `[scope] → []` | Make scope current |

#### 1.2.4 Binding Operations (Static Names)

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `BIND` | `[value, sym] → []` | Bind in current scope |
| `LOOKUP` | `[sym] → [value]` | Search scope chain |
| `LOOKUP_HERE` | `[sym] → [value]` | Current scope only |
| `SET` | `[value, sym] → []` | Update in scope chain |

#### 1.2.5 Binding Operations (Dynamic Names)

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `BIND_DYN` | `[value, str] → []` | Intern string, bind |
| `LOOKUP_DYN` | `[str] → [value]` | Intern string, lookup (external arrays first) |
| `SET_DYN` | `[value, str] → []` | Intern string, set |
| `INVOKE_DYN` | `[args..., val] → [result]` | Runtime type dispatch: call closure or index array |
| `ARRAY_SET_DYN` | `[val, idx, str] → []` | Set element in external array by name |

Dynamic binding enables runtime-computed variable names like `\lambda<f\valueof<i>>`.

**`LOOKUP_DYN` resolution order:**
1. External array callback (if registered) - checks interpreter's array registry
2. Local scope chain - falls back to Subnivean's internal bindings

**`INVOKE_DYN` behavior:**
- If target is a closure: calls with argc arguments
- If target is an array and argc=1: indexes the array
- Otherwise: runtime error

#### 1.2.6 Cell Operations

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `CELL_NEW` | `[v] → [cell]` | Box value into cell |
| `CELL_GET` | `[cell] → [v]` | Unbox (copy out) |
| `CELL_SET` | `[v, cell] → []` | Replace cell contents |
| `CELL_INC` | `[cell] → [n]` | Increment, return new value |
| `CELL_DEC` | `[cell] → [n]` | Decrement, return new value |

#### 1.2.7 Arithmetic

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `ADD` | `[a, b] → [a+b]` | Addition |
| `SUB` | `[a, b] → [a-b]` | Subtraction |
| `MUL` | `[a, b] → [a*b]` | Multiplication |
| `DIV` | `[a, b] → [a/b]` | Integer division |
| `MOD` | `[a, b] → [a%b]` | Modulo |
| `NEG` | `[a] → [-a]` | Negation |

Note: For binary operations, `b` is on top of stack (popped first).

#### 1.2.8 Comparison

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `EQ` | `[a, b] → [0/1]` | Equal |
| `NE` | `[a, b] → [0/1]` | Not equal |
| `LT` | `[a, b] → [0/1]` | Less than |
| `GT` | `[a, b] → [0/1]` | Greater than |
| `LE` | `[a, b] → [0/1]` | Less or equal |
| `GE` | `[a, b] → [0/1]` | Greater or equal |

#### 1.2.9 Logic

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `AND` | `[a, b] → [0/1]` | Logical AND (non-short-circuit) |
| `OR` | `[a, b] → [0/1]` | Logical OR (non-short-circuit) |
| `NOT` | `[a] → [0/1]` | Logical NOT |

Truthiness rules:
- `nil` → false
- `0` → false
- `""` (empty string) → false
- `[]` (empty array) → false
- Everything else → true

#### 1.2.10 Control Flow

| Opcode | Operand | Stack Effect | Description |
|--------|---------|--------------|-------------|
| `JUMP` | offset | — | Unconditional jump |
| `JUMP_IF` | offset | `[cond] → []` | Jump if truthy |
| `JUMP_UNLESS` | offset | `[cond] → []` | Jump if falsy |
| `LOOP` | offset | — | Backward jump |

Jump offsets are relative to the *next* instruction:
- `JUMP 0` = no-op (jump to next instruction)
- `JUMP 5` = skip 5 instructions
- `LOOP 10` = go back 10 instructions

#### 1.2.11 Functions and Closures

| Opcode | Operand | Stack Effect | Description |
|--------|---------|--------------|-------------|
| `CLOSURE` | func_idx | `[scope] → [closure]` | Create closure |
| `CALL` | argc | `[args..., closure] → [result]` | Call closure |
| `TAIL_CALL` | argc | `[args..., closure] → [result]` | Tail call optimization |
| `RETURN` | — | — | Return nil |
| `RETURN_VAL` | — | `[value] → ` | Return value |

Calling convention:
1. Push arguments left-to-right (first arg deepest)
2. Push closure
3. Execute `CALL n` where n = argument count
4. Arguments are bound to parameters in the new scope
5. Return value replaces everything on stack

#### 1.2.12 Arrays

| Opcode | Operand | Stack Effect | Description |
|--------|---------|--------------|-------------|
| `ARRAY_NEW` | count | `[elems...] → [array]` | Create from stack |
| `ARRAY_GET` | — | `[arr, idx] → [elem]` | Get element |
| `ARRAY_SET` | — | `[arr, idx, val] → []` | Set element |
| `ARRAY_LEN` | — | `[arr] → [len]` | Get length |
| `ARRAY_PUSH` | — | `[arr, val] → []` | Append |
| `ARRAY_POP` | — | `[arr] → [val]` | Remove last |
| `COPYARRAY` | — | `[src_name_str, dest_sym] → []` | Copy external array to local |

**`COPYARRAY` operation:**
1. Pops source name string (e.g., `"_m0_grid"`)
2. Fetches external array via callback (single bulk copy)
3. Creates a new local Subnivean array with the elements
4. Binds it to the destination symbol in current scope

This enables O(1) random access to arrays that would otherwise require O(N) external lookups per access.

#### 1.2.13 Heap Memory (Persistent Store)

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `MEM_LOAD` | `[addr, idx] → [value]` | Load from heap block |
| `MEM_STORE` | `[addr, idx, val] → []` | Store to heap block |
| `MEM_LEN` | `[addr] → [len]` | Get length of heap block |
| `MEM_ALLOC` | `[count] → [addr]` | Allocate new block |

**Purpose:**

The heap memory opcodes provide access to a persistent array store that survives across VM calls. Unlike regular Subnivean arrays (which are garbage-collected values), heap blocks are:

1. **Persistent**: Survive VM reset and multiple function calls
2. **Addressable**: Identified by integer addresses (0, 1, 2, ...)
3. **Shared**: Accessible from both Subnivean VM and the host interpreter

**Use case: Zero-copy array sharing**

In applications like Game of Life, the grid array needs to be:
- Modified by computational lambdas (running in Subnivean)
- Read for display (running in the interpreter)

Without heap memory, arrays must be copied between the interpreter and VM on every frame. With heap memory:

```tex
% Create persistent array (interpreter side)
\let<grid>#{[0,0,1,0,0,1,1,1,0]}  % Returns address, e.g., 0

% Access from computational lambda (VM side)
\lambda<STEP>[grid_addr, w, h]#{
    \let<i>{0}
    \begin{loop}
        \exit_when{\ge{\valueof<i>,\mul{\valueof<w>,\valueof<h>}}}

        % Read current cell value
        \let<val>{\mem_load{\valueof<grid_addr>,\valueof<i>}}

        % ... compute new value ...

        % Write updated cell value
        \mem_store{\valueof<grid_addr>,\valueof<i>,\valueof<new_val>}

        \inc<i>
    \end{loop}
}

% Access from interpreter for display
\sn_array{\valueof<grid>}[idx]  % Reads heap[addr][idx]
```

**Hyades syntax for heap operations inside computational lambdas:**

| Command | Compiles To | Description |
|---------|-------------|-------------|
| `\mem_load{addr,idx}` | `MEM_LOAD` | Load `heap[addr][idx]`, returns value |
| `\mem_store{addr,idx,val}` | `MEM_STORE` | Store `val` to `heap[addr][idx]` |
| `\mem_len{addr}` | `MEM_LEN` | Get length of heap block |
| `\mem_alloc{count}` | `MEM_ALLOC` | Allocate block with `count` zeros, returns addr |

**Memory model:**

- Heap is a growable array of blocks: `heap[addr] → Block`
- Each block is a growable array of int64 values: `Block[idx] → int64`
- Addresses are assigned sequentially starting from 0
- Blocks persist until `sn_store_free_all()` is called (typically at program exit)

**Stack effects (detailed):**

```
MEM_LOAD:   [..., addr, idx] → [..., heap[addr][idx]]
MEM_STORE:  [..., addr, idx, val] → [...] ; heap[addr][idx] = val
MEM_LEN:    [..., addr] → [..., len(heap[addr])]
MEM_ALLOC:  [..., count] → [..., new_addr] ; allocates block with `count` zeros
```

#### 1.2.14 Maps (Robin Hood Hash Tables)

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `MAP_NEW` | `[] → [addr]` | Create empty map in persistent store |
| `MAP_GET` | `[addr, key] → [value]` | Get value (0 if missing) |
| `MAP_SET` | `[addr, key, val] → []` | Set key-value pair |
| `MAP_HAS` | `[addr, key] → [0/1]` | Check if key exists |
| `MAP_DEL` | `[addr, key] → [0/1]` | Delete key, return if existed |
| `MAP_LEN` | `[addr] → [count]` | Number of entries |
| `MAP_KEYS` | `[addr] → [arr_addr]` | Get array of all keys |

**Purpose:**

Maps provide O(1) average-case key-value storage using Robin Hood hashing. Like heap arrays, maps live in the persistent store and are identified by integer addresses.

**Robin Hood hashing:**

Robin Hood hashing is an open-addressing scheme that maintains low variance in probe sequence lengths by "stealing" from rich slots (short probes) to give to poor slots (long probes). This results in:
- O(1) average lookup, insert, and delete
- Excellent cache locality
- Low memory overhead (no linked lists)

**Hyades syntax for map operations:**

| Command | Description |
|---------|-------------|
| `\let<name>#{|k:v, k:v, ...|}` | Create map with initial key-value pairs |
| `\let<name\|\|>{|k:v, k:v, ...|}` | Alternative syntax with `\|\|` suffix |
| `\map_get<name>{key}` | Get value (returns 0 if key missing) |
| `\map_set<name>{key, value}` | Set or update key-value pair |
| `\map_has<name>{key}` | Check if key exists (returns 0 or 1) |
| `\map_del<name>{key}` | Delete key (returns 1 if existed, 0 if not) |
| `\map_len<name>` | Get number of entries |
| `\map_keys<name>` | Get heap address of array containing all keys |

**Example usage:**

```tex
% Create a map with initial values
\let<scores>#{|1:85, 2:92, 3:78|}

% Access values
Player 1 score: \map_get<scores>{1}    % → 85

% Update a value
\map_set<scores>{1, 95}
Updated score: \map_get<scores>{1}     % → 95

% Check existence before access
\if{\map_has<scores>{4}}{
    Score: \map_get<scores>{4}
}\else{
    Player 4 not found
}

% Delete an entry
\map_del<scores>{2}
Remaining entries: \map_len<scores>    % → 2

% Iterate over keys
\let<keys>{\map_keys<scores>}
% keys now holds heap address of [1, 3] array
```

**Use cases:**

- **Sparse grids**: Store only non-zero cells (e.g., sparse Game of Life)
- **Caching**: Memoization of computed values
- **Counters**: Count occurrences of values
- **Lookups**: Fast key-based data retrieval

**Implementation details:**

- Keys and values are 64-bit integers
- Load factor threshold: 80% (triggers resize)
- Initial capacity: 16 slots
- Growth factor: 2× on resize
- Hash function: splitmix64
- Deletion: backward-shift (no tombstones)

#### 1.2.15 Strings

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `CONCAT` | `[a, b] → [a+b]` | Concatenate strings |
| `STRINGIFY` | `[v] → [str]` | Convert to string |
| `SYMBOL` | `[str] → [sym]` | Intern string as symbol |

#### 1.2.16 Output

| Opcode | Operand | Stack Effect | Description |
|--------|---------|--------------|-------------|
| `OUTPUT` | — | `[v] → []` | Stringify and append to output |
| `OUTPUT_RAW` | const_idx | — | Emit literal from constant pool |

#### 1.2.17 Special

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `HALT` | — | Stop execution |
| `BREAKPOINT` | — | Debugger hook |
| `ASSERT` | `[cond, msg] → []` | Error if falsy |

### 1.3 Execution Model

#### 1.3.1 Machine State

The VM maintains:
- **Value Stack**: Operand stack for computation
- **Call Stack**: Stack of call frames for function calls
- **Current Scope**: The active lexical environment
- **Global Scope**: The root environment
- **Output Buffer**: Accumulated output string
- **Symbol Table**: Interned symbols

#### 1.3.2 Call Frames

Each call frame contains:
- **Closure**: The function being executed
- **IP**: Instruction pointer into the function's bytecode
- **BP**: Base pointer (stack position at call time)
- **Scope**: The scope for this activation

#### 1.3.3 Function Objects

A function contains:
- **Name**: For debugging
- **Arity**: Number of parameters
- **Parameters**: Array of symbols
- **Bytecode**: Array of instructions
- **Constants**: Pool of literals (integers, strings, symbols, nested functions)

#### 1.3.4 Constant Pool

Each function has a constant pool containing:
- `CONST_INT`: Integer literals
- `CONST_STRING`: String literals
- `CONST_SYMBOL`: Symbol references
- `CONST_FUNCTION`: Nested function definitions

### 1.4 Scope Semantics

Subnivean's scope system is designed to support:
1. **Lexical scoping**: Variables resolve to their definition site
2. **Closures**: Functions capture their defining environment
3. **Mutable cells**: Capture-by-reference semantics
4. **Per-iteration scopes**: Fresh bindings for each loop iteration

#### 1.4.1 Scope Chain

Scopes form a chain through parent pointers:

```
Global Scope
    ↑
Function Scope
    ↑
Block Scope
    ↑
Current Scope
```

`LOOKUP` searches up the chain; `BIND` creates bindings in the current scope.

#### 1.4.2 Capture-by-Reference

When a closure captures a variable that holds a cell:

```
\let<x>{0}           ; x = Cell{0}
\lambda<get>{\valueof<x>}  ; captures scope containing x
\inc<x>              ; x = Cell{1}
\recall<get>         ; returns 1 (sees the mutation)
```

The closure captures the *scope*, which contains a binding to the *cell*. The cell is shared, so mutations are visible.

#### 1.4.3 Per-Iteration Scopes

The "JavaScript closure in loop" problem:

```javascript
// JavaScript (broken)
for (var i = 0; i < 3; i++) {
    funcs[i] = () => i;
}
// All functions return 3!
```

Subnivean solves this with per-iteration scopes:

```
SCOPE_NEW              ; Create iteration scope
  ; Copy current i value to NEW cell in this scope
  LOOKUP i
  CELL_GET
  CELL_NEW
  BIND i               ; Shadow i with fresh cell

  SCOPE_CAPTURE
  CLOSURE func         ; Closure captures iteration scope
SCOPE_POP              ; End iteration scope
```

Each closure captures its own iteration scope with its own cell.

### 1.5 External Callbacks (Hyades Integration)

Subnivean supports external callbacks that allow the VM to access resources managed by the host interpreter. This enables Subnivean to execute computational lambdas that reference arrays and lambdas defined in Hyades' scope.

#### 1.5.1 Callback Types

| Callback | Signature | Purpose |
|----------|-----------|---------|
| `array_lookup` | `(ctx, name) → elements[]` | Retrieve array from interpreter |
| `array_set` | `(ctx, name, idx, val) → ok` | Write element to interpreter's array |
| `lambda_compile` | `(ctx, name) → Function*` | Compile external lambda on demand |

#### 1.5.2 Array Lookup

When `OP_LOOKUP_DYN` encounters a string name, it first queries the external array lookup callback before checking the local scope. This allows patterns like:

```tex
\lambda<COUNT_NEIGHBORS>[grid, x, y]#{
    % grid is a string like "_m0_grid" (hygienized array name)
    % \recall<\recall<grid>>[idx] accesses the interpreter's array
}
```

The callback returns array elements as strings, which are converted to Subnivean integers.

#### 1.5.3 Array Set

For indirect array writes (`\setelement<\recall<name>>[idx]{val}`), the VM uses `OP_ARRAY_SET_DYN` which calls the external array set callback. This enables modifying arrays that exist in the interpreter's scope:

```tex
\lambda<STEP>[src, dst, w, h]#{
    % src and dst are hygienized array names
    \setelement<\recall<dst>>[\valueof<i>]{\valueof<val>}
}
```

#### 1.5.4 Lambda Compilation

When `OP_LOOKUP` encounters an unknown symbol and the external lambda compile callback is registered, Subnivean requests compilation of the lambda from the interpreter. The compiled function is cached in Subnivean's global scope for subsequent calls.

This enables cross-lambda calls:

```tex
\lambda<STEP>[grid, w, h]#{
    % COUNT_NEIGHBORS is another lambda defined in Hyades
    \let<neighbors>{\recall<COUNT_NEIGHBORS>[\recall<grid>, ...]}
}
```

#### 1.5.5 Hygiene Integration

Hyades applies hygiene prefixes (`_m0_`, `_m1_`, etc.) during macro expansion. When Subnivean receives arguments:

- **Numeric strings** (e.g., `"42"`, `"-7"`) → converted to `VAL_INT`
- **Non-numeric strings** (e.g., `"_m0_grid"`) → kept as `VAL_STRING` for indirect lookup

The `\ref<name>` command in Hyades returns the hygienized name as a string, enabling:

```tex
\recall<STEP>[\ref<src>, \ref<dst>, ...]
```

### 1.6 Memory Management

Subnivean uses reference counting for memory management.

#### 1.5.1 Reference-Counted Types

- Strings
- Cells
- Arrays
- Closures
- Scopes

#### 1.5.2 Ownership Rules

1. **Creating a value**: Refcount starts at 1
2. **Copying a value** (`value_copy`): Increments refcount
3. **Freeing a value** (`value_free`): Decrements refcount
4. **Binding to scope**: Scope takes ownership (no incref needed)
5. **Looking up**: Returns a copy (caller owns it)

#### 1.5.3 Cycle Handling

Cycles can occur when scopes reference closures that capture those scopes. Currently, Subnivean does not handle cycles automatically — they will leak. In practice, typical Hyades programs don't create cycles.

---

## Part 2: Implementation Overview

### 2.1 File Organization

```
subnivean/v2/
├── value.h        # Value type definitions
├── value.c        # Value operations, strings, symbols, cells, arrays
├── opcode.h       # Instruction set definition
├── scope.h        # Scope type definition
├── scope.c        # Scope operations, binding hashtable
├── function.h     # Function and Closure definitions
├── function.c     # Function operations, constant pool, disassembler
├── vm.h           # VM state definition
├── vm.c           # Main interpreter loop
├── test_vm.c      # Unit tests with hand-crafted bytecode
├── test_gol.c     # Game of Life integration test
└── bench_vm.c     # Performance benchmark
```

### 2.2 Core Data Structures

#### 2.2.1 Value (value.h)

```c
typedef enum {
    VAL_NIL, VAL_INT, VAL_STRING, VAL_SYMBOL,
    VAL_CELL, VAL_ARRAY, VAL_CLOSURE, VAL_SCOPE
} ValueKind;

typedef struct {
    ValueKind kind;
    union {
        int64_t as_int;
        String *as_string;
        Symbol *as_symbol;
        Cell *as_cell;
        Array *as_array;
        Closure *as_closure;
        Scope *as_scope;
    };
} Value;
```

The tagged union is 16 bytes: 8 for the tag (with padding) and 8 for the payload.

#### 2.2.2 String

```c
typedef struct String {
    char *data;      // UTF-8 bytes (null-terminated for convenience)
    size_t len;      // Length in bytes
    int refcount;
} String;
```

Strings are immutable after creation.

#### 2.2.3 Symbol

```c
typedef struct Symbol {
    const char *name;   // Interned name
    size_t len;
    uint32_t hash;      // Cached hash for fast comparison
    struct Symbol *next; // Hash bucket chain
} Symbol;
```

Symbols are interned in a hash table. Two symbols with the same name are the same pointer.

#### 2.2.4 Cell

```c
typedef struct Cell {
    Value value;     // Contained value
    int refcount;
} Cell;
```

Cells are the only mutable value container.

#### 2.2.5 Scope

```c
typedef struct Scope {
    struct Scope *parent;  // Lexical parent
    Binding **buckets;     // Hash table of bindings
    size_t n_buckets;
    size_t n_bindings;
    int refcount;
    uint32_t id;           // For debugging
    const char *tag;       // Optional label
} Scope;

typedef struct Binding {
    Symbol *name;
    Value value;
    struct Binding *next;  // Hash bucket chain
} Binding;
```

Scopes use a hash table for O(1) average lookup within a scope.

#### 2.2.6 Function

```c
typedef struct Function {
    char *name;
    uint32_t id;
    int arity;
    Symbol **params;      // Parameter names

    Instruction *code;    // Bytecode
    int code_len;
    int code_cap;

    Constant *constants;  // Constant pool
    int n_constants;
    int constants_cap;

    int refcount;
} Function;
```

#### 2.2.7 Closure

```c
typedef struct Closure {
    Function *func;
    Scope *captured;      // Scope at definition time
    int refcount;
} Closure;
```

#### 2.2.8 VM State

```c
typedef struct VM {
    // Value stack
    Value stack[VM_STACK_MAX];
    Value *sp;

    // Call stack
    CallFrame frames[VM_FRAMES_MAX];
    int frame_count;

    // Scopes
    Scope *global;
    Scope *scope;

    // Symbol table
    SymbolTable symbols;

    // Output buffer
    char *output;
    size_t output_len;
    size_t output_cap;

    // State
    bool running;
    bool had_error;
    char error_msg[256];
} VM;
```

### 2.3 Interpreter Loop

The main interpreter is a switch-based dispatch loop in `vm.c`:

```c
static bool run(VM *vm) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

    while (vm->running) {
        Instruction *inst = frame->ip++;
        uint32_t op = inst->op;
        int32_t operand = inst->operand;

        switch (op) {
            case OP_PUSH_INT:
                vm_push(vm, value_int(operand));
                break;

            case OP_ADD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, value_int(value_to_int(a) + value_to_int(b)));
                value_free(a);
                value_free(b);
                break;
            }

            case OP_CALL: {
                Value callee = vm_pop(vm);
                if (!call_closure(vm, callee.as_closure, operand)) {
                    return false;
                }
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            // ... ~60 more cases
        }
    }
    return true;
}
```

Key implementation details:

1. **Instruction fetch**: Single struct access, then advance IP
2. **Stack operations**: Direct pointer manipulation (`vm->sp++`)
3. **Value ownership**: Pop takes ownership, push transfers ownership
4. **Frame switching**: On CALL, update local `frame` pointer

### 2.4 Memory Management Details

#### 2.4.1 Value Lifecycle

```c
// Create values (refcount = 1)
Value value_int(int64_t n);
Value value_string(String *s);  // Takes ownership
Value value_cell(Cell *c);      // Increfs

// Copy (increments refcount)
Value value_copy(Value v);

// Free (decrements refcount)
void value_free(Value v);
```

#### 2.4.2 Stack Operations

```c
void vm_push(VM *vm, Value v) {
    *vm->sp++ = v;  // Transfer ownership to stack
}

Value vm_pop(VM *vm) {
    return *--vm->sp;  // Transfer ownership to caller
}
```

#### 2.4.3 Scope Binding

```c
void scope_bind(Scope *s, Symbol *name, Value v) {
    // v ownership transferred to scope
    // Old value (if rebinding) is freed
}

bool scope_lookup(Scope *s, Symbol *name, Value *out) {
    // Returns a COPY - caller owns it
    *out = value_copy(binding->value);
    return true;
}
```

### 2.5 Bytecode Generation

Functions are built using a builder API:

```c
Function *f = function_new("add", 2);

// Set parameters
f->params[0] = vm_intern(vm, "a");
f->params[1] = vm_intern(vm, "b");

// Add symbols to constant pool
int a_idx = function_add_symbol(f, f->params[0]);
int b_idx = function_add_symbol(f, f->params[1]);

// Emit bytecode
function_emit(f, OP_PUSH_CONST, a_idx);
function_emit_simple(f, OP_LOOKUP);
function_emit(f, OP_PUSH_CONST, b_idx);
function_emit_simple(f, OP_LOOKUP);
function_emit_simple(f, OP_ADD);
function_emit_simple(f, OP_RETURN_VAL);
```

#### 2.5.1 Jump Patching

Forward jumps require patching:

```c
int jump_addr = function_emit_jump(f, OP_JUMP_IF);  // Placeholder
// ... emit then-branch ...
function_patch_jump(f, jump_addr);  // Patches to current position
```

#### 2.5.2 Loop Construction

```c
int loop_start = function_offset(f);
// ... emit loop body ...
// ... emit exit condition and JUMP_IF to exit ...
function_emit_loop(f, loop_start);  // Backward jump
function_patch_jump(f, exit_jump);  // Patch exit
```

### 2.6 Disassembler

The disassembler produces human-readable bytecode:

```
== STEP (arity=2) ==
0000  SCOPE_CAPTURE
0001  CLOSURE        8
0002  PUSH_CONST     10 (#COUNT_NEIGHBORS)
0003  BIND
0004  PUSH_INT       0
0005  CELL_NEW
0006  PUSH_CONST     2 (#_r)
0007  BIND
0008  PUSH_CONST     2 (#_r)
0009  LOOKUP
0010  CELL_GET
0011  PUSH_INT       5
0012  GE
0013  JUMP_IF        76 -> 90
...
```

### 2.7 Performance Characteristics

#### 2.7.1 Time Complexity

| Operation | Complexity |
|-----------|------------|
| Stack push/pop | O(1) |
| Symbol comparison | O(1) (pointer equality) |
| Scope lookup | O(d) where d = scope depth |
| Binding lookup in scope | O(1) average (hash table) |
| Array access | O(1) |
| String concatenation | O(n+m) |

#### 2.7.2 Space Complexity

| Structure | Size |
|-----------|------|
| Value | 16 bytes |
| Instruction | 8 bytes |
| Scope (empty) | ~40 bytes + hash table |
| String | 24 bytes + data |
| Cell | 24 bytes |

#### 2.7.3 Benchmark Results

**Game of Life (5×5 grid, 1000 iterations):**

| Implementation | Time | Speedup |
|----------------|------|---------|
| Subnivean 2.0 VM | 0.118s | **65×** |
| Hyades Interpreter | 7.63s | 1× |

**Game of Life with External Callbacks:**

When running computational lambdas that access interpreter-managed arrays via callbacks:

| Pattern | Description |
|---------|-------------|
| `\recall<\recall<grid>>[idx]` | Double-indirection array read |
| `\setelement<\recall<dst>>[idx]{val}` | Indirect array write |
| `\recall<COUNT_NEIGHBORS>[...]` | Cross-lambda call |

All patterns execute at near-native bytecode speed despite crossing the Subnivean/Hyades boundary. The callback overhead is minimal compared to the interpreter's text-based expansion.

**With `\copyarray` optimization:**

| Version | Time | Improvement |
|---------|------|-------------|
| External lookups only | ~35ms | baseline |
| With `\copyarray` | ~23ms | **1.5× faster** |

The `\copyarray` command fetches an external array once and creates a local copy for O(1) random access, avoiding repeated O(N) external lookups.

#### 2.7.4 Lambda Call Overhead in Computational Lambdas

When writing computational lambdas (`#{...}`), prefer **inlining simple expressions** over calling helper lambdas. Each `\recall<...>` involves:

1. Looking up the lambda by name
2. Setting up a new VM call frame
3. Passing arguments
4. Executing the function
5. Returning the result

**Example: Key encoding for sparse Game of Life**

```hyades
% SLOW: Lambda call per encoding
\lambda<ENCODE_KEY>[row,col]#{
    \return{\add{\mul{\valueof<row>,10000},\valueof<col>}}
}

% In a loop (called hundreds of times):
\let<key>{\recall<ENCODE_KEY>[\valueof<nr>,\valueof<nc>]}
```

```hyades
% FAST: Inline the expression directly
\let<key>{\add{\mul{\valueof<nr>,10000},\valueof<nc>}}
```

The arithmetic operations (`\add`, `\mul`, `\div`, `\mod`) compile to single VM opcodes, much faster than a full lambda call.

**Impact Example:**

A sparse Game of Life with 5 live cells and ~45 candidate cells per step:

| Approach | Lambda Calls per Step |
|----------|----------------------|
| Nested helper lambdas | ~555 calls |
| Inlined expressions | ~0 calls |

The nested approach used `ENCODE_KEY`, `DECODE_ROW`, `DECODE_COL`, `COUNT_NEIGHBORS`, and `NEXT_STATE` lambdas, each called multiple times in nested loops. This caused the simulation to appear frozen.

**Guidelines:**

1. **Inline trivial operations** like coordinate encoding (`row * 10000 + col`)
2. **Keep helper lambdas** for complex, reusable logic (e.g., sorting, complex algorithms)
3. **Profile before optimizing** - lambda calls are still fast for moderate call counts
4. **Watch for nested loops** - O(n²) lambda calls can become problematic

### 2.8 Future Directions

#### 2.8.1 Potential Optimizations

1. **Computed goto dispatch**: Replace switch with GCC's computed goto
2. **NaN boxing**: Pack values into 64 bits using NaN payload
3. **Inline caching**: Cache scope lookups for hot variables
4. **JIT compilation**: Generate native code for hot functions
5. **Cycle detection**: Add mark-and-sweep for cyclic references

#### 2.8.2 Planned Features

1. **Maps/Dictionaries**: First-class hash maps
2. **Exceptions**: Try/catch with stack unwinding
3. **Tail call optimization**: Proper TCO for recursive functions
4. **Debugging**: Source maps, breakpoints, stepping
5. **Hyades compiler**: Full compilation from Hyades source to Subnivean bytecode

---

## Appendix A: Opcode Quick Reference

```
Stack:    NOP POP DUP SWAP ROT
Const:    PUSH_NIL PUSH_TRUE PUSH_FALSE PUSH_INT PUSH_CONST
Scope:    SCOPE_NEW SCOPE_POP SCOPE_CAPTURE SCOPE_RESTORE
Bind:     BIND LOOKUP LOOKUP_HERE SET
Dynamic:  BIND_DYN LOOKUP_DYN SET_DYN INVOKE_DYN ARRAY_SET_DYN
Cell:     CELL_NEW CELL_GET CELL_SET CELL_INC CELL_DEC
Arith:    ADD SUB MUL DIV MOD NEG
Compare:  EQ NE LT GT LE GE
Logic:    AND OR NOT
Control:  JUMP JUMP_IF JUMP_UNLESS LOOP
Func:     CLOSURE CALL TAIL_CALL RETURN RETURN_VAL
Array:    ARRAY_NEW ARRAY_GET ARRAY_SET ARRAY_LEN ARRAY_PUSH ARRAY_POP COPYARRAY
Heap:     MEM_LOAD MEM_STORE MEM_LEN MEM_ALLOC
Map:      MAP_NEW MAP_GET MAP_SET MAP_HAS MAP_DEL MAP_LEN MAP_KEYS
String:   CONCAT STRINGIFY SYMBOL
Output:   OUTPUT OUTPUT_RAW
Special:  HALT BREAKPOINT ASSERT
```

## Appendix B: Hyades to Subnivean Mapping

### B.1 Basic Operations

| Hyades | Subnivean |
|--------|-----------|
| `\let<x>{5}` | `PUSH_INT 5; CELL_NEW; PUSH_CONST #x; BIND` |
| `\valueof<x>` | `PUSH_CONST #x; LOOKUP; CELL_GET` |
| `\inc<x>` | `PUSH_CONST #x; LOOKUP; CELL_INC` |
| `\assign<y>{v}` | `<v>; PUSH_CONST #y; BIND` (no cell) |
| `\recall<y>` | `PUSH_CONST #y; LOOKUP` |
| `\lambda<f>[a]{body}` | `SCOPE_CAPTURE; CLOSURE f; PUSH_CONST #f; BIND` |
| `\recall<f>[x]` | `<x>; PUSH_CONST #f; LOOKUP; CALL 1` |
| `\if{c}{t}\else{e}` | `<c>; JUMP_UNLESS else; <t>; JUMP end; else: <e>; end:` |
| `\begin{loop}...\exit_when{c}...\end{loop}` | `loop: ...; <c>; JUMP_IF exit; ...; LOOP loop; exit:` |

### B.2 Array Operations

| Hyades | Subnivean |
|--------|-----------|
| `\let<arr[]>{[1,2,3]}` | `PUSH 1; PUSH 2; PUSH 3; ARRAY_NEW 3; PUSH_CONST #arr; BIND` |
| `\recall<arr>[i]` | `PUSH_CONST #arr; LOOKUP; <i>; ARRAY_GET` |
| `\setelement<arr>[i]{v}` | `PUSH_CONST #arr; LOOKUP; <i>; <v>; ARRAY_SET` |

### B.3 Indirect/Dynamic Operations

These patterns enable double-indirection where a parameter holds the name of another variable:

| Hyades | Subnivean |
|--------|-----------|
| `\ref<name>` | `PUSH_CONST "name"` (returns hygienized name as string) |
| `\recall<\recall<x>>` | `<x>; STRINGIFY; LOOKUP_DYN` (lookup by computed name) |
| `\recall<\recall<arr>>[i]` | `<i>; <arr>; STRINGIFY; LOOKUP_DYN; INVOKE_DYN 1` |
| `\setelement<\recall<arr>>[i]{v}` | `<arr>; STRINGIFY; <i>; <v>; ARRAY_SET_DYN` |

**Use case:** Computational lambdas that operate on arrays passed by reference:

```tex
\lambda<STEP>[src, dst, w, h]#{
    % src = "_m0_grid", dst = "_m0_next" (hygienized names)
    \let<val>{\recall<\recall<src>>[\valueof<idx>]}  % Read from src array
    \setelement<\recall<dst>>[\valueof<idx>]{\valueof<val>}  % Write to dst array
}

% Called with array references:
\recall<STEP>[\ref<grid>, \ref<next>, 5, 5]
```

### B.4 Array Copying (Performance Optimization)

| Hyades | Subnivean |
|--------|-----------|
| `\copyarray<LOCAL>{\recall<arr_ref>}` | `PUSH_CONST #LOCAL; <arr_ref>; STRINGIFY; COPYARRAY` |

**Use case:** Copy an external array to a local Subnivean array for fast random access:

```tex
\lambda<COUNT_NEIGHBORS>[grid, w, h, row, col]#{
    % Copy external array to local (one bulk fetch)
    \copyarray<LOCALGRID>{\recall<grid>}

    % Now all accesses are O(1) local lookups instead of O(N) external callbacks
    \let<cell>{\recall<LOCALGRID>[\valueof<idx>]}
    ...
}
```

**Performance impact:**
- Without `\copyarray`: Each `\recall<\recall<grid>>[i]` fetches all N elements, then indexes
- With `\copyarray`: One bulk fetch, then O(1) local array access

### B.5 Map Operations

| Hyades | Subnivean |
|--------|-----------|
| `\let<m>#{|k:v,...|}` | `MAP_NEW; <k>; <v>; MAP_SET; ...; PUSH_CONST #m; BIND` |
| `\map_get<m>{k}` | `PUSH_CONST #m; LOOKUP; <k>; MAP_GET` |
| `\map_set<m>{k,v}` | `PUSH_CONST #m; LOOKUP; <k>; <v>; MAP_SET` |
| `\map_has<m>{k}` | `PUSH_CONST #m; LOOKUP; <k>; MAP_HAS` |
| `\map_del<m>{k}` | `PUSH_CONST #m; LOOKUP; <k>; MAP_DEL` |
| `\map_len<m>` | `PUSH_CONST #m; LOOKUP; MAP_LEN` |
| `\map_keys<m>` | `PUSH_CONST #m; LOOKUP; MAP_KEYS` |

**Use case:** Sparse data structures with O(1) key-based access:

```tex
% Count character frequencies
\let<freq>#{||}                         % Empty map
\let<i>{0}
\begin{loop}
    \exit_when{\ge{\valueof<i>,\len<text>}}
    \let<ch>{\recall<text>[\valueof<i>]}
    \let<count>{\map_get<freq>{\valueof<ch>}}
    \map_set<freq>{\valueof<ch>, \add{\valueof<count>,1}}
    \inc<i>
\end{loop}
```

### B.6 Compiler Name Resolution

When the compiler encounters `\recall<name>[args]`, it must decide between:
- **Array access** (`LOOKUP` + `ARRAY_GET`)
- **Function call** (`LOOKUP` + `CALL`)
- **Runtime dispatch** (`LOOKUP` + `INVOKE_DYN`)

Resolution strategy:
1. If `name` is a known array (declared with `\let<name[]>`): emit `ARRAY_GET`
2. If `name` is a known lambda: emit `CALL`
3. If `name` is unknown at compile time (e.g., created by `\copyarray`): emit `INVOKE_DYN`

`INVOKE_DYN` handles both cases at runtime:
- If the looked-up value is a closure → call it
- If it's an array and argc=1 → index it

---

## Appendix C: New CL Syntax (Computational Lambda Syntax 2.0)

The Computational Lambda (CL) syntax inside `#{}` blocks has been redesigned for clarity and reduced verbosity. Both old and new syntax are supported; old syntax remains for backward compatibility.

### C.1 Syntax Summary

| Old Syntax | New Syntax | Description |
|------------|------------|-------------|
| `\valueof<x>` | `${x}` | Variable value access |
| `\recall<x>` (value) | `${x}` | Variable value access |
| `\recall<fn>[args]` | `\invoke<fn>[args]` | Function call |
| `\recall<arr>[idx]` | `\at<arr>[idx]` | Array element read |
| `\setelement<arr>[idx]{val}` | `\set<arr>[idx]{val}` | Array element write |
| `\map_get{addr, key}` | `\at<*addr>[key]` | Map read via dereference |
| `\map_set{addr, key, val}` | `\set<*addr>[key]{val}` | Map write via dereference |
| `[a, b, c]` | `[a:int, b:int, c:int]` | Typed parameters |
| `\let<x>{val}` | `\let<x:type>{val}` | Typed variable declaration |

### C.2 Type Annotations

Types are specified at declaration time:

```tex
\let<x:int>{5}                    %%% Integer
\let<name:string>{hello}          %%% String
\let<arr:int[]>{[1, 2, 3]}        %%% Integer array
\let<m:map>{|1->10, 2->20|}       %%% Map
\let<p:address>{\addressof<arr>}  %%% Address/reference
```

Reassignment omits the type (already known):
```tex
\let<x>{10}                       %%% x was declared as int
```

Lambda parameters require types:
```tex
\lambda<add>[a:int, b:int]#{
    \return{\add{${a}, ${b}}}
}
```

### C.3 Variable Access: `${name}`

The `${}` syntax replaces both `\valueof<>` and `\recall<>` for value access:

```tex
\let<x:int>{5}
\let<y:int>{\add{${x}, 3}}        %%% y = 8
```

Dynamic variable names (runtime-computed):
```tex
\let<i:int>{2}
\let<val:int>{${item${i}}}        %%% accesses "item2"
```

### C.4 Function Calls: `\invoke<fn>[args]`

```tex
\lambda<double>[n:int]#{
    \return{\mul{${n}, 2}}
}

\let<result:int>{\invoke<double>[5]}   %%% result = 10
```

Dynamic dispatch (function name from variable):
```tex
\invoke<${fn_name}>[args]
```

### C.5 Collection Access: `\at<>` and `\set<>`

Unified read/write for arrays and maps:

```tex
%%% Arrays
\let<arr:int[]>{[10, 20, 30]}
\let<x:int>{\at<arr>[0]}          %%% x = 10
\set<arr>[1]{99}                   %%% arr = [10, 99, 30]

%%% Maps (via address dereference)
\let<m:map>{|1->100, 2->200|}
\let<v:int>{\at<*m>[1]}           %%% v = 100  (dereference m, get key 1)
\set<*m>[3]{300}                   %%% add key 3 -> 300
```

The `*` prefix means "dereference": the variable holds an address, access the thing it points to.

### C.6 Before/After Example

**Old syntax:**
```tex
\lambda<RENDER>[cols,rows,buf_addr]#{
    \let<r>{1}
    \begin{loop}
        \exit_when{\gt{\valueof<r>,\recall<rows>}}
        \let<c>{0}
        \begin{loop}
            \exit_when{\ge{\valueof<c>,\recall<cols>}}
            \let<packed>{\mem_load{\recall<buf_addr>,
                \add{\mul{\sub{\valueof<r>,1},\recall<cols>},\valueof<c>}}}
            \if{\gt{\valueof<packed>,0}}{
                \cursor{\valueof<r>,\add{\mul{\valueof<c>,2},1}}
                \emit{\recall<K>[\valueof<ch>]}
            }
            \inc<c>
        \end{loop}
        \inc<r>
    \end{loop}
}
```

**New syntax:**
```tex
\lambda<RENDER>[cols:int, rows:int, buf_addr:address]#{
    \let<r:int>{1}
    \begin{loop}
        \exit_when{\gt{${r}, ${rows}}}
        \let<c:int>{0}
        \begin{loop}
            \exit_when{\ge{${c}, ${cols}}}
            \let<packed:int>{\mem_load{${buf_addr},
                \add{\mul{\sub{${r}, 1}, ${cols}}, ${c}}}}
            \if{\gt{${packed}, 0}}{
                \cursor{${r}, \add{\mul{${c}, 2}, 1}}
                \emit{\at<K>[${ch}]}
            }
            \inc<c>
        \end{loop}
        \inc<r>
    \end{loop}
}
```

Key improvements:
- `${r}` vs `\valueof<r>` — 4 chars vs 12 chars
- `${cols}` vs `\recall<cols>` — 7 chars vs 14 chars
- `\at<K>[${ch}]` vs `\recall<K>[\valueof<ch>]` — clearer intent
- Typed parameters — self-documenting
- ~30% shorter, significantly more readable

### C.7 Bytecode Mapping

The new syntax compiles to identical bytecode as the old syntax:

| New Syntax | Compiles To | Same As Old... |
|------------|-------------|----------------|
| `${x}` | `LOOKUP` + `CELL_GET` | `\valueof<x>` |
| `\invoke<f>[args]` | `CALL` or `INVOKE_DYN` | `\recall<f>[args]` |
| `\at<arr>[idx]` | `LOOKUP` + `ARRAY_GET` | `\recall<arr>[idx]` |
| `\at<*addr>[key]` | `LOOKUP` + `MEM_LOAD` | `\map_get{addr, key}` |
| `\set<arr>[idx]{val}` | `ARRAY_SET` | `\setelement<arr>[idx]{val}` |
| `\set<*addr>[key]{val}` | `MAP_SET` | `\map_set{addr, key, val}` |

No performance difference — purely syntactic improvement.

### C.8 Migration Notes

- **Backward compatible**: Old syntax continues to work
- **Mixing allowed**: Both syntaxes can coexist in the same file (but not recommended for style consistency)
- **Interpreter unchanged**: Outside `#{}` blocks, use old syntax (`\recall`, `\valueof`)
- **Type annotations optional for reassignment**: First `\let` requires type, subsequent assignments don't
