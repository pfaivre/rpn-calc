# Agent Guidelines for rpn-calc

This document provides guidelines for agentic coding tools working in the rpn-calc repository.

## Build System

The project uses a simple Makefile-based build system:

- **Build command**: `make`
- **Clean command**: `make clean`
- **Static analysis**: `make check` (uses cppcheck)
- **Run with valgrind**: `make valgrind`

The build system compiles C code using GCC 14 or newer with the C23 standard and includes GMP library for arbitrary precision arithmetic.

## Code Style Guidelines

### General C Style
- Follow the C23 standard
- Use GCC compiler flags: `-Wall -pedantic -std=c23`
- Use C23 facilities where they help: checked arithmetic from `<stdckdint.h>` for every size computation that feeds an allocation, and the standard attributes `[[nodiscard]]` (results that must be used) and `[[noreturn]]` (fatal error helpers)
- Include MIT License header in all source files
- Use clear, descriptive comments for functions and complex logic

### Naming Conventions
- **Functions**: Use snake_case (e.g., `_calc_binary_op`, `stack_push`); a few legacy helpers predate this rule and keep their camelCase names (e.g., `_calc_proceed`, `bufferedInput`)
- **Variables**: Use snake_case (e.g., `mpz_t number`, `Stack s`)
- **Constants**: Use UPPER_CASE (e.g., `STDIN_BUFFER_SIZE`)
- **Types**: Use CamelCase for structs (e.g., `StructStack`), typedefs use the struct name (e.g., `Stack`)
- **Private functions**: Prefix with underscore and keep them `static` (e.g., `_calc_binary_op`); declare only the public API in headers

### Formatting
- Indentation: 4 spaces in C source; tabs in the makefile (required by make)
- Braces: Opening brace on same line, closing brace on new line
- Spacing: Use spaces around operators, after commas
- Line length: No strict limit, but keep lines reasonable (< 100 chars recommended)

### Error Handling
- Check for NULL pointers and memory allocation failures
- Use `fputs()` for fixed error messages to stderr (`fprintf()` when the message is formatted)
- Exit with non-zero status on critical errors
- For stack operations, check minimum required elements before operations

### Memory Management
- Always free allocated memory
- Use `mpz_clear()` for GMP integers when done
- Check return values of memory allocation functions
- Provide cleanup functions (e.g., `stack_delete()`)

### Header Files
- Include guards: Use `#ifndef HEADER_H` pattern
- Organize includes: Standard library first, then local headers
- Forward declare types when possible
- Keep header files minimal with only necessary declarations

### Documentation
- Use Javadoc-style comments for functions:
```c
/**
 * Brief description
 * @param param1 Description
 * @param param2 Description
 * @return Description of return value
 */
```
- Add TODO comments for future improvements
- Document complex algorithms and non-obvious logic

## Testing

The project uses Github Workflows for continuous integration.

There are no unit tests in the traditional sense. Testing is done through:
1. Manual testing of the compiled binary
2. Static analysis with cppcheck
3. Memory checking with valgrind

## Development Workflow

1. **Build**: Run `make` to compile
2. **Test**: Run `make check` for static analysis
3. **Test**: Run `make test` for functional testing
4. **Run**: Execute `./rpn` with appropriate arguments
5. **Memory check**: Run valgrind to check for memory issues. Example: `valgrind ./rpn -e "12 _3 4 ^ + 11 / v 22 - f"`

## Review principles (thermos)

Branch audits run a bug/security pass and a code-quality pass over the diff.
Write new code as if it will be audited against these rubrics:

- Be ambitious about simplification ("code judo"): prefer the restructure
  that deletes whole branches, helpers, modes, or layers over one that merely
  moves the same complexity around. If a path exists to remove complexity
  rather than rearrange it, take it.
- No spaghetti growth: ad-hoc conditionals or one-off special cases bolted
  onto unrelated flows are a design problem, not a stylistic nit — push the
  logic into a dedicated helper, state machine, policy object, or module.
- Type and boundary cleanliness: no unnecessary `Option`, `any`, or casts
  where a clearer type boundary exists; explicit typed models over
  loosely-shaped ad-hoc objects; no silent fallbacks papering over unclear
  invariants.
- Keep logic in its canonical layer: no feature-specific code leaking into
  shared paths, no bespoke one-offs where a canonical helper already exists,
  and related state updates atomic rather than spread across a flow.
- Correctness first: no panics on user-controlled input; error paths return
  `Result` and are handled at the boundary; every behavior change is
  deliberate and covered by a test.
- "It works" is not approval. Presumptive blockers unless justified: a file
  pushed from under to over 1k lines, scattered feature checks in shared
  code, an added indirection layer that clarifies nothing, duplicated logic
  instead of an extracted helper.
- Do not let changes push a file from under 1k lines to over 1k lines without
  a very strong reason. Prefer extracting helpers, subcomponents, modules, or
  local abstractions. If the diff crosses that threshold, explicitly ask whether
  the code should be decomposed first. Only waive this with a compelling
  structural reason where the file is still clearly organized.

## Project Structure

- `main.c`: Entry point and command-line parsing
- `calc.c/calc.h`: Calculator logic and RPN parsing
- `stack.c/stack.h`: Stack of tagged values (decimal number or string)
- `number.c/number.h`: Decimal number arithmetic (value = num / 10^scale) and printing
- `registers.c/registers.h`: Register file (per-register value stacks and arrays)
- `io.c/io.h`: Input/output utilities
- `makefile`: Build configuration

## Special Notes

- The project uses GMP (GNU Multiple Precision Arithmetic Library)
- Negative numbers are represented with underscore prefix (e.g., `_5`)
- The calculator supports basic RPN operations: +, -, *, /, %, ~, ^, v, c, p, n, P, f, d, r, x, q, Q, k, K, i, I, o, O, decimal numbers (1.5, .5), string literals [...], registers (s, l, S, L, :, ;) and conditionals (> < = !> !< != with a register name)
- Error messages are written to stderr
- The codebase follows defensive programming practices

## Agent-Specific Instructions

1. Always maintain the existing code style
2. Preserve the MIT license headers
3. Follow the existing naming conventions
4. Add appropriate error handling
5. Document new functions with Javadoc-style comments
6. Run static analysis before committing changes
7. Always test the program using tests/functional_tests.sh
