# rpn-calc [![Build and test](https://github.com/pfaivre/rpn-calc/actions/workflows/build-test.yml/badge.svg?branch=master)](https://github.com/pfaivre/rpn-calc/actions/workflows/build-test.yml)
A humble dc clone

## Synopsis
```sh
rpn [-V] [--version] [-h] [--help]
    [-e scriptexpression] [--expression=scriptexpression]
```

## Description
This is a calculator that uses the reverse-polish notation (RPN).
It stores the numbers in a stack and operates with the top ones until the
desired result is reached.

For example the calculation `(3 + 5) * 7` can be noted as `3 5 + 7 *` in RPN.

See below for a complete set of commands.

Thanks to [GMP](https://gmplib.org/), it supports arbitrary precision
arithmetic.

## Options
rpn may be invoked with the following command-line options:

```sh
-V, --version
```
Print out the version of rpn that is being run and a copyright notice, then
exit.

```sh
-h, --help
```
Print a usage message briefly summarizing these command-line options, then exit.

```sh
-e, --expression=script
```
Add the commands in script to the set of commands to be run. Multiple `-e`
options are concatenated.

## Input
Without `-e`, expressions are read from the standard input, one per line; the
stack persists across lines. Spaces, tabs and newlines separate tokens, and
spaces may be omitted between tokens (`2 3*4+p`). Negative numbers are written
with an underscore prefix (`_5`). A `#` starts a comment that runs to the end
of the line. A NUL byte ends the line for the parser: everything after the
first NUL byte is ignored.

The exit status is 0 when every command succeeded, and 1 if any command failed
(errors are reported on stderr) or an unknown option was given.

## Installation
Building requires GCC 14 or newer (C23 support: `-std=c23` and `<stdckdint.h>`).

This program depends on GNU MP library version 6 or above. It is available with
the package `libgmp-dev` on Debian and derivative, as `gmp-devel` on Fedora or
as `gmp` on Arch Linux.

```sh
$ make
$ ./rpn
```

## Commands
`[...]` Pushes a string literal onto the stack. The outer brackets are not part of the string; inner bracket pairs are kept, so nesting works (`[[a]b]` pushes `[a]b`). Spaces, `#` and digits are allowed inside, and the literal may be written without separating spaces (`2[ab]p`). A literal not closed on the same line is an error.

`p` Prints the top value of the stack. A string is printed as-is.

`n` Pops the top value of the stack and prints it without a trailing newline. A string is printed as-is.

`P` Pops the top value of the stack and prints it without a trailing newline. A string is printed as-is; a number is printed as a string of bytes, most significant byte of its absolute value first (so `72 105 256 * + P` prints `iH`).

`+` Pops two values off the stack, adds them and pushes the result on the stack.

`-` Pops two values off the stack, subtracts the first popped from the second popped and pushes the result on the stack.

`*` Pops two values off the stack, multiplies them and pushes the result on the stack.

`/` Pops two values off the stack, divides the second popped by the first popped and pushes the quotient on the stack.

`%` Pops two values off the stack, divides the second popped by the first popped and pushes the remainder on the stack.

`~` Pops two values off the stack, divides the second one popped by the first one popped. The quotient is pushed first, and the remainder is pushed next.

`^` Pops two values off the stack, use the first as the exponent and the second as the base and pushes the result on the stack. A negative exponent gives the integer part of the exact result, as dc does at scale 0: 0 unless the base is 1 or _1 (which give 1 or _1 according to the exponent's parity). A positive exponent whose result would exceed 32 MiB is rejected with an error and the operands stay on the stack.

`v` Pops one value, computes its square root, and pushes the result to the stack.

`d` Duplicates the top value of the stack.

`r` Swaps the two top values of the stack. With fewer than two values on the stack it does nothing (as in dc).

`c` Clears the stack.

`f` Prints the entire stack without modifying it.

`x` If the top value is a string, pops it and executes its contents as commands on the same stack (macros can nest: `[[2 3 + p] x] x` prints `5`). A number is left untouched, not popped.

`s r` Pops the top value of the main stack and stores it in register `r`, replacing its previous content. The register name is the next token, written with or without a separating space (`sa` and `s a` both name register `a`).

`l r` Pushes a copy of the top of register `r` onto the main stack. An empty register loads 0.

`S r` Pops the top value of the main stack and pushes it onto register `r`'s own stack.

`L r` Pops the top of register `r`'s stack and pushes it onto the main stack. It is an error if that register stack is empty.

`: r` Pops two values off the main stack and stores the second popped (the value, which may be a string) at the index given by the first popped (the top) in register `r`'s array, replacing any previous cell at that index. The index must be a nonnegative integer.

`; r` Pops the index off the main stack and pushes the value stored at that index in register `r`'s array, or 0 if the index was never stored.

Registers are named by a single character (256 of them, case-sensitive, digits allowed) and persist across input lines. A register's value stack (`s`/`l`/`S`/`L`) and its array (`:`/`;`) are independent.

## Compatibility with dc
For the commands listed above, rpn behaves like dc (checked against GNU dc 1.5.2):

- The RPN stack model: values are pushed on a stack, operators pop their
  operands, and the stack persists across input lines.
- Number syntax: decimal integers, `_` prefix for negative numbers, leading
  zeros, tokens may be written without separating spaces (`2 3*4+p`, and
  `2_3` is read as 2 then _3), and `#` starts a comment running to the end
  of the line.
- Operator semantics: `/`, `%` and `~` truncate toward zero and the remainder
  takes the sign of the dividend (`~` pushes the quotient first, then the
  remainder); `^` with a non-negative exponent, including `0 0 ^` = 1, and a
  negative exponent giving the integer part of the exact result, as dc does
  at scale 0; `v` rounds down; division by zero leaves the operands on the
  stack and the session continues; `r` with fewer than two values does
  nothing; `p` prints the top value without popping it and `f` prints the
  whole stack, top first; `n` pops and prints without a trailing newline and
  `P` pops and prints the number as bytes, most significant byte first.
- Arbitrary-precision integer arithmetic: results are exact, as in dc.
- String literals `[...]` (with nested brackets), and printing them with `p`, `n`, `P` and `f`: `p` adds a trailing newline, `n` and `P` pop the value and print without one, `f` prints one value per line, top first. Arithmetic on a string reports `non-numeric value` and leaves the operands on the stack, as in dc.
- Macro execution with `x`: a string is popped and run as commands on the same stack, macros nest, and `x` on a number leaves the stack untouched.
- Registers: `s`/`l`/`S`/`L` on a per-register value stack (with `s` replacing the content, `l` peeking, `L` erroring on an empty register stack, and `l` on an empty register loading 0), and `:`/`;` arrays per register (index on top, missing index loads 0, negative or non-numeric index rejected with the operands kept, values may be strings, a register's stack and array are independent).
- The `-e` option, and reading expressions from standard input without it.

rpn deliberately differs from dc in the following ways:

- Error messages are prefixed `rpn:` instead of `dc:`, and a stack underflow
  reads `stack underflow` where dc says `stack empty`.
- The exit status is 1 if any command failed, where dc always exits 0. The
  session still continues after an error in both programs.
- Integers only: no decimal point, no scale (`k`) and no input/output base
  (`i`, `o`) — input and output are always base 10.
- No conditionals or loops: an unknown command such as `>a` is reported as
  unimplemented, where dc would treat it as a conditional operation. Strings
  can be pushed, printed, executed with `x` and stored in registers, but
  there is no `q`/`Q` to abort a macro early. A `[...]` literal not closed
  on the same line is an error, where dc keeps reading the following lines
  into the string.
- The register name is the next token, so `s a` names register `a`, where dc
  reads the very next character and would name a register after the space.
  Array indices are arbitrary precision, where dc 1.5.2 punts on indices
  beyond an unsigned long.

- A positive exponent that does not fit in an unsigned long, or whose result
  would exceed 32 MiB, is rejected with `exponent too large in raise`, while
  a huge negative exponent is computed exactly (0, 1 or _1), where dc gives
  up with the same error.

## Limitations
This program is much more basic than dc and currently lacks some notable features like:
- decimal numbers
- conditionals and loops

## License
Copyright 2016-2026 Pierre Faivre. This is free software, and may be redistributed
under the terms specified in the LICENSE file.
