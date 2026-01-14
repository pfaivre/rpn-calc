# Agent Guidelines for rpn-calc

This document provides guidelines for agentic coding tools working in the rpn-calc repository.

## Build System

The project uses a simple Makefile-based build system:

- **Build command**: `make`
- **Clean command**: `make clean`
- **Static analysis**: `make check` (uses cppcheck)
- **Run with valgrind**: `make valgrind`

The build system compiles C code using GCC with C99 standard and includes GMP library for arbitrary precision arithmetic.

## Code Style Guidelines

### General C Style
- Follow C99 standard
- Use GCC compiler flags: `-Wall -pedantic -std=c99`
- Include MIT License header in all source files
- Use clear, descriptive comments for functions and complex logic

### Naming Conventions
- **Functions**: Use snake_case (e.g., `_calc_proceed`, `stack_push`)
- **Variables**: Use snake_case (e.g., `mpz_t number`, `Stack s`)
- **Constants**: Use UPPER_CASE (e.g., `STDIN_BUFFER_SIZE`)
- **Types**: Use CamelCase for structs (e.g., `StructStack`), typedefs use the struct name (e.g., `Stack`)
- **Private functions**: Prefix with underscore (e.g., `_calc_tokenThreshold`)

### Formatting
- Indentation: Use tabs (4 spaces equivalent)
- Braces: Opening brace on same line, closing brace on new line
- Spacing: Use spaces around operators, after commas
- Line length: No strict limit, but keep lines reasonable (< 100 chars recommended)

### Error Handling
- Check for NULL pointers and memory allocation failures
- Use `fputs()` for error messages to stderr
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

The project uses Travis CI for continuous integration:
- **Test command**: `make clean && make check && make`
- **Static analysis**: `cppcheck -q --std=c99 *.c *.h`

There are no unit tests in the traditional sense. Testing is done through:
1. Manual testing of the compiled binary
2. Static analysis with cppcheck
3. Memory checking with valgrind

## Development Workflow

1. **Build**: Run `make` to compile
2. **Test**: Run `make check` for static analysis
3. **Run**: Execute `./rpn` with appropriate arguments
4. **Memory check**: Run `make valgrind` to check for memory issues

## Project Structure

- `main.c`: Entry point and command-line parsing
- `calc.c/calc.h`: Calculator logic and RPN parsing
- `stack.c/stack.h`: Stack implementation
- `io.c/io.h`: Input/output utilities
- `makefile`: Build configuration

## Special Notes

- The project uses GMP (GNU Multiple Precision Arithmetic Library)
- Negative numbers are represented with underscore prefix (e.g., `_5`)
- The calculator supports basic RPN operations: +, -, *, /, %, ^, v, c, p, f, d, r
- Error messages are written to stderr
- The codebase follows defensive programming practices

## Agent-Specific Instructions

1. Always maintain the existing code style
2. Preserve the MIT license headers
3. Follow the existing naming conventions
4. Add appropriate error handling
5. Document new functions with Javadoc-style comments
6. Run static analysis before committing changes
7. Test manually with various RPN expressions
