# WAH (WebAssembly in a Header) Project Overview

This project implements a WebAssembly (WASM) interpreter entirely within a single C header file (`wah.h`). It provides the necessary data structures, parsing logic, and execution engine to run WebAssembly modules.

## Implementation Strategy

The WAH interpreter employs a two-pass approach for efficient WebAssembly module execution:

1. **Pre-parsing:** After initial parsing of the WASM binary, the bytecode for each function undergoes a two-pass "pre-parsing" phase (`wah_preparse_code`) to optimize it for interpretation.

   - **Pass 1: Analysis and Layout.** This pass scans the raw WASM bytecode to calculate the final size of the optimized code and resolve all jump targets. It identifies control flow blocks (`block`, `loop`, `if`, `else`). For backward-branching `loop` blocks, the jump target is their own starting instruction, which is recorded immediately. For forward-branching `block` and `if` blocks, the jump target is the location of their corresponding `end` instruction (or `else` for an `if`). These forward-jump targets are resolved and recorded only when the `else` or `end` instruction is encountered during the scan. This pass effectively lays out the structure of the final code and resolves all jump destinations before the second pass begins.

   - **Pass 2: Code Generation.** This pass generates the final, optimized bytecode. It iterates through the raw WASM code again, emitting a new stream of instructions. Variable-length LEB128 encoded arguments are decoded and replaced with fixed-size native types (e.g., `uint32_t`). Branch instructions (`br`, `br_if`, `br_table`) are written with the pre-calculated absolute jump offsets from Pass 1. Structural opcodes like `block`, `loop`, and `end` (except for the final one) are removed entirely, as their control flow logic is now embedded directly into the jump offsets of the branch instructions. This process eliminates the need for runtime LEB128 decoding and complex control flow resolution, significantly speeding up interpretation.

2. **Custom Call Stack:** Unlike traditional compiled code that relies on the host system's call stack, WAH implements its own explicit call stack (`wah_call_frame_t` within `wah_exec_context_t`). This custom stack manages function call frames, instruction pointers, and local variable offsets. This design choice ensures portability across different environments and provides fine-grained control over the execution state, preventing host stack overflow issues and enabling features like stack inspection if needed.

3. **Interpreter Dispatch Strategy:** The WAH interpreter supports two main dispatch strategies for executing pre-parsed bytecode, controlled by the `WAH_USE_MUSTTAIL` macro:

   - **Switch-based Interpreter:** This traditional approach uses a central `while` loop that fetches each opcode and dispatches to a large `switch` statement. Each `case` within the `switch` handles a specific opcode's logic. While straightforward to implement, it can incur overhead from branch prediction misses and the `switch` statement itself.

   - **Computed Goto Interpreter:** When `WAH_USE_COMPUTED_GOTO` is defined (automatically enabled for GCC/Clang compilers), the interpreter utilizes a "computed goto" or "direct threading" dispatch mechanism. This approach creates a jump table where each entry is the address of the code that implements a specific opcode. After an opcode's logic is executed, the interpreter directly jumps to the next opcode's handler by looking up its address in the jump table. This method significantly reduces dispatch overhead compared to a switch-based interpreter by eliminating the need for repeated `switch` statement evaluations and improving branch prediction. It offers performance benefits similar to direct threaded code but uses `goto *address;` instead of tail calls.

   - **Direct Threaded Code Interpreter:** When `WAH_USE_MUSTTAIL` is defined (automatically enabled for recent enough GCC/Clang), the interpreter leverages C compiler extensions (like GCC/Clang's `__attribute__((musttail))`) for **tail call optimization**. Instead of a central `switch` loop, each opcode's logic is encapsulated in a dedicated function (e.g., `wah_run_I32_CONST` for `WAH_OP_I32_CONST`). After an opcode is executed, the `WAH_NEXT()` macro performs a tail-recursive call to a dispatcher function (`wah_run_single`), which then tail-calls the next opcode's handler. This effectively transforms function calls into direct jumps between opcode handlers, significantly reducing dispatch overhead and improving performance by:
     - Minimizing stack usage, as new stack frames are not pushed for each instruction dispatch.
     - Improving branch prediction, as the flow of control is more direct.
     This strategy is generally faster for interpreters due to its reduced overhead compared to switch-based dispatch.

## Error Handling

All public API functions return a `wah_error_t` enum value. `WAH_OK` indicates success, while other values signify specific errors such as invalid WASM format, out-of-memory conditions, or runtime traps. Use `wah_strerror(err)` to get a human-readable description of an error.

## Development Notes

- The interpreter is designed to be self-contained within `wah.h` by defining `WAH_IMPLEMENTATION` in one C/C++ file.
- It uses standard C libraries (`stdint.h`, `stddef.h`, `stdbool.h`, `string.h`, `stdlib.h`, `assert.h`, `math.h`, `stdio.h`).
- Platform-specific intrinsics (`__builtin_popcount`, `_BitScanReverse`, etc.) are used for performance where available, with generic fallbacks.
- **Manual WASM Binary Creation:** When manually crafting WASM binaries (without tools like `wat2wasm`), extreme care must be taken with section size calculations. Always double-check that the initial size numbers are correct. During testing, explicitly verify that the result code is *not* `WAH_ERROR_UNEXPECTED_EOF` to catch early errors related to incorrect section sizing.
- **Numeric Overflow Checks:** During arithmetic operations inside parsing (e.g., addition, multiplication), it is crucial to explicitly check for potential numeric overflows. If an overflow occurs, it must be reported using the `WAH_ERROR_TOO_LARGE` error code to indicate that the result exceeds the representable range. Note that this doesn't apply to runtime numeric operations which assume 2's complements.

### Testing

- To run all tests in Windows, execute `test.bat` (or simply `test`) from the command line. To run tests that start with a specific prefix (e.g., `wah_test_control_flow.c`), execute `test <prefix>` (e.g., `test control_flow`, NOT `test wah_test_control_flow` nor `test test_control_flow`).
- To run all tests in POSIX-like environments, execute `make test` from the command line. To run tests with a specific name, execute `make test_control_flow` for `wah_test_control_flow.c` and so on.
- **Debugging with WAH_DEBUG:** For debugging purposes, you can declare the `WAH_DEBUG` macro. When running test scripts, you can enable this by executing `test -g ...` instead of `test ...`.
  - When `WAH_DEBUG` is enabled, `WAH_LOG` allows you to print logs in a `(line number) content` format using `printf`-like syntax.
  - Similarly, `WAH_CHECK` and similar macros will automatically output the failure location and error codes using `WAH_LOG` when `WAH_DEBUG` is active.
  - For debugging logs, it is best to use `WAH_LOG` exclusively due to the aforementioned reasons. In particular do not expand `WAH_CHECK` for logging.
- Strive to use an existing test file for new test cases. Introduce a new file only when a new major category is warranted.
- **IMPORTANT:** Any new bug should introduce a failing regression test that still does compile, so that the fix is demonstrated to reliably make it pass. Do not code the fix first!
