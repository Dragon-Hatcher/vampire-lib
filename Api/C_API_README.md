# Vampire C API

This directory contains a plain C API for using Vampire as a library, suitable for Foreign Function Interface (FFI) bindings from languages like Python, Rust, Go, JavaScript, and others.

## Files

- `vampire_c_api.h` - C header file with all API declarations
- `vampire_c_api.cpp` - Implementation that wraps the C++ API
- `c_api_example.c` - Example C program demonstrating the API
- `VampireAPI.hpp` / `VampireAPI.cpp` - Original C++ API (wrapped by the C API)

## Features

- **FFI-Friendly**: All types are opaque pointers or plain C types (int, bool, char*)
- **Self-Contained**: Single header file with all declarations
- **Memory Safe**: Vampire manages memory for returned objects
- **Complete**: Covers term construction, formula building, clause creation, and proving

## Quick Start (C)

```c
#include "vampire_c_api.h"

int main(void) {
    // Initialize
    vampire_init();
    vampire_set_time_limit(10);

    // Register symbols
    unsigned int a = vampire_add_function("a", 0);
    unsigned int P = vampire_add_predicate("P", 1);

    // Build terms and formulas
    vampire_term_t* const_a = vampire_constant(a);
    vampire_literal_t* Pa = vampire_lit(P, true, &const_a, 1);
    vampire_formula_t* formula = vampire_atom(Pa);

    // Create and solve problem
    vampire_unit_t* axiom = vampire_axiom_formula(formula);
    vampire_unit_t* units[] = {axiom};
    vampire_problem_t* problem = vampire_problem_from_units(units, 1);

    vampire_proof_result_t result = vampire_prove(problem);

    return (result == VAMPIRE_PROOF) ? 0 : 1;
}
```

## Compilation

The C API is automatically built as part of the `vampire_lib` static library when you build Vampire.

### Building Vampire with the C API

```bash
# From the Vampire root directory
mkdir build
cd build
cmake ..
make vampire_lib  # Builds the library including C API
```

### Building the C API Example

```bash
# From the build directory
make c_api_example
./c_api_example
```

### Using the C API in Your Project

Once built, you can link against `libvampire_lib.a`:

```bash
# Compile your C program
gcc -o my_program my_program.c \
    -I/path/to/vampire \
    -L/path/to/vampire/build \
    -lvampire_lib \
    -lstdc++ -lpthread -lm
```

Note: Even though the API is C, you need to link with C++ libraries (`-lstdc++`) because Vampire is implemented in C++.

## FFI Examples

### Python (using ctypes)

```python
from ctypes import *

# Load the library
vampire = CDLL("libvampire.so")

# Define function signatures
vampire.vampire_init.argtypes = []
vampire.vampire_init.restype = None

vampire.vampire_add_function.argtypes = [c_char_p, c_uint]
vampire.vampire_add_function.restype = c_uint

vampire.vampire_constant.argtypes = [c_uint]
vampire.vampire_constant.restype = c_void_p

# Use the API
vampire.vampire_init()
vampire.vampire_set_time_limit(10)

a = vampire.vampire_add_function(b"a", 0)
const_a = vampire.vampire_constant(a)

# ... build and prove
```

### Rust (using FFI)

```rust
use std::ffi::CString;
use std::os::raw::{c_char, c_uint, c_void};

#[link(name = "vampire")]
extern "C" {
    fn vampire_init();
    fn vampire_add_function(name: *const c_char, arity: c_uint) -> c_uint;
    fn vampire_constant(functor: c_uint) -> *mut c_void;
    fn vampire_set_time_limit(seconds: i32);
}

fn main() {
    unsafe {
        vampire_init();
        vampire_set_time_limit(10);

        let name = CString::new("a").unwrap();
        let a = vampire_add_function(name.as_ptr(), 0);
        let const_a = vampire_constant(a);

        // ... build and prove
    }
}
```

### Go (using cgo)

```go
package main

// #cgo LDFLAGS: -lvampire
// #include "vampire_c_api.h"
import "C"
import "unsafe"

func main() {
    C.vampire_init()
    C.vampire_set_time_limit(10)

    name := C.CString("a")
    defer C.free(unsafe.Pointer(name))

    a := C.vampire_add_function(name, 0)
    constA := C.vampire_constant(a)

    // ... build and prove
}
```

### Node.js (using ffi-napi)

```javascript
const ffi = require('ffi-napi');
const ref = require('ref-napi');

const voidPtr = ref.refType(ref.types.void);

const vampire = ffi.Library('libvampire', {
  'vampire_init': ['void', []],
  'vampire_set_time_limit': ['void', ['int']],
  'vampire_add_function': ['uint', ['string', 'uint']],
  'vampire_constant': [voidPtr, ['uint']],
  // ... other functions
});

vampire.vampire_init();
vampire.vampire_set_time_limit(10);

const a = vampire.vampire_add_function("a", 0);
const constA = vampire.vampire_constant(a);

// ... build and prove
```

## API Structure

### Initialization
- `vampire_init()` - Initialize library
- `vampire_reset()` - Full reset (clears signature)
- `vampire_prepare_for_next_proof()` - Light reset between proofs

### Symbol Registration
- `vampire_add_function(name, arity)` - Register function symbol
- `vampire_add_predicate(name, arity)` - Register predicate symbol

### Term Construction
- `vampire_var(index)` - Create variable
- `vampire_constant(functor)` - Create constant
- `vampire_term(functor, args, count)` - Create function application

### Literal Construction
- `vampire_eq(positive, lhs, rhs)` - Create equality/disequality
- `vampire_lit(pred, positive, args, count)` - Create predicate literal
- `vampire_neg(literal)` - Negate literal

### Formula Construction
- `vampire_atom(literal)` - Atomic formula
- `vampire_not(formula)` - Negation
- `vampire_and(formulas, count)` - Conjunction
- `vampire_or(formulas, count)` - Disjunction
- `vampire_imp(lhs, rhs)` - Implication
- `vampire_iff(lhs, rhs)` - Equivalence
- `vampire_forall(var_index, formula)` - Universal quantification
- `vampire_exists(var_index, formula)` - Existential quantification

### Problem and Proving
- `vampire_problem_from_units(units, count)` - Create problem
- `vampire_prove(problem)` - Run prover
- `vampire_get_refutation()` - Get proof
- `vampire_extract_proof(refutation, out_steps, out_count)` - Get structured proof

### String Conversions
- `vampire_term_to_string(term, buffer, size)` - Convert term to string
- `vampire_literal_to_string(literal, buffer, size)` - Convert literal to string
- `vampire_clause_to_string(clause, buffer, size)` - Convert clause to string

## Memory Management

The Vampire C API follows these memory management rules:

1. **Returned Opaque Pointers**: All returned pointers to Vampire objects (terms, literals, formulas, clauses, etc.) are managed by Vampire. Do NOT free them.

2. **String Conversions**: The `*_to_string()` functions require a pre-allocated buffer. You allocate the buffer, pass it in, and Vampire writes to it.

3. **Proof Extraction**: The `vampire_extract_proof()` function allocates memory that you must free using `vampire_free_proof_steps()`.

4. **Literals Array**: The `vampire_get_literals()` function allocates memory that you must free using `vampire_free_literals()`.

5. **Input Strings**: When you pass strings to Vampire (e.g., symbol names), Vampire copies them internally. You can free your copy after the call.

## Error Handling

Most functions return:
- `-1` on error for functions returning `int`
- `NULL` on error for functions returning pointers
- Special enum values (e.g., `VAMPIRE_UNKNOWN`) for result enums

Always check return values before using results.

## Thread Safety

The Vampire library is **not thread-safe**. If you need to use Vampire from multiple threads:

1. Protect all API calls with a mutex, OR
2. Create separate processes (not threads) for parallel proving

## Tips for FFI Bindings

1. **Opaque Pointers**: All `vampire_*_t*` types are opaque. In your FFI, treat them as `void*` or equivalent.

2. **Arrays**: C arrays are passed as pointer + count. In your language, you'll need to convert native arrays/lists to C arrays.

3. **Booleans**: C `bool` is usually compatible with language booleans, but may need conversion.

4. **Enums**: C enums are integers. Use your language's enum or constant types.

5. **String Buffers**: For `*_to_string()` functions, allocate a buffer (e.g., 1024 bytes), pass it in, and check the return value.

## Example Use Case

The C API is ideal for:
- Embedding Vampire in applications written in other languages
- Building web services that use Vampire for theorem proving
- Creating interactive proof assistants
- Integrating Vampire into larger automated reasoning systems
- Teaching and research tools that need programmatic access to Vampire

## Further Reading

- See `c_api_example.c` for a complete working example
- See `VampireAPI.hpp` for the underlying C++ API
- Consult the Vampire documentation for proving strategies and options
