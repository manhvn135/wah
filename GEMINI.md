# WAH (WebAssembly in a Header) Project Overview

This project implements a WebAssembly (WASM) interpreter entirely within a single C header file (`wah.h`). It provides the necessary data structures, parsing logic, and execution engine to run WebAssembly modules.

## Implementation Strategy

The WAH interpreter employs a two-pass approach for efficient WebAssembly module execution:

1. **Pre-parsing:** After initial parsing of the WASM binary, the bytecode undergoes a "pre-parsing" phase. During this phase, variable-length LEB128 encoded arguments are decoded and converted into fixed-size native types (e.g., `uint32_t`, `int32_t`). Control flow instructions (`if`, `else`, `br`, `br_if`, `loop`, `block`) are analyzed, and their jump targets are resolved and stored as direct offsets. This eliminates the need for runtime LEB128 decoding and control flow analysis, significantly speeding up interpretation. Instructions that are purely for validation (like `block` and `loop` opcodes themselves) are removed from the executable bytecode stream.

2. **Custom Call Stack:** Unlike traditional compiled code that relies on the host system's call stack, WAH implements its own explicit call stack (`wah_call_frame_t` within `wah_exec_context_t`). This custom stack manages function call frames, instruction pointers, and local variable offsets. This design choice ensures portability across different environments and provides fine-grained control over the execution state, preventing host stack overflow issues and enabling features like stack inspection if needed.

## Error Handling

All public API functions return a `wah_error_t` enum value. `WAH_OK` indicates success, while other values signify specific errors such as invalid WASM format, out-of-memory conditions, or runtime traps. Use `wah_strerror(err)` to get a human-readable description of an error.

## Development Notes

- The interpreter is designed to be self-contained within `wah.h` by defining `WAH_IMPLEMENTATION` in one C/C++ file.
- It uses standard C libraries (`stdint.h`, `stddef.h`, `stdbool.h`, `string.h`, `stdlib.h`, `assert.h`, `math.h`, `stdio.h`).
- Platform-specific intrinsics (`__builtin_popcount`, `_BitScanReverse`, etc.) are used for performance where available, with generic fallbacks.
- **Manual WASM Binary Creation:** When manually crafting WASM binaries (without tools like `wat2wasm`), extreme care must be taken with section size calculations. Always double-check that the initial size numbers are correct. During testing, explicitly verify that the result code is *not* `WAH_ERROR_UNEXPECTED_EOF` to catch early errors related to incorrect section sizing.
- **Numeric Overflow Checks:** During arithmetic operations (e.g., addition, multiplication), it is crucial to explicitly check for potential numeric overflows. If an overflow occurs, it must be reported using the `WAH_ERROR_TOO_LARGE` error code to indicate that the result exceeds the representable range.

### Testing

- To run all tests, execute `test.bat` (or simply `test`) from the command line.
- To run tests that start with a specific prefix (e.g., `wah_test_control_flow.c`), execute `test <prefix>` (e.g., `test control_flow`, NOT `test wah_test_control_flow` nor `test test_control_flow`).
- **Debugging with WAH_DEBUG:** For debugging purposes, you can declare the `WAH_DEBUG` macro. When running test scripts, you can enable this by executing `test -g ...` instead of `test ...`.
  - When `WAH_DEBUG` is enabled, `WAH_LOG` allows you to print logs in a `(line number) content` format using `printf`-like syntax.
  - Similarly, `WAH_CHECK` and similar macros will automatically output the failure location and error codes using `WAH_LOG` when `WAH_DEBUG` is active.
  - For debugging logs, it is best to use `WAH_LOG` exclusively due to the aforementioned reasons. In particular do not expand `WAH_CHECK` for logging.
