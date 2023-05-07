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
Add the commands in script to the set of commands to be run while processing 
the input.

If any command-line parameters remain after processing the above, these 
parameters are interpreted as the names of input files to be processed. A file 
name of - refers to the standard input stream. The standard input will 
processed if no script files or expressions are specified.

## Installation
This program depends on GNU MP library version 6 or above. It is available with 
the package `libgmp-dev` on Debian and derivative, as `gmp-devel` on Fedora or
as `gmp` on Arch Linux.

```sh
$ make
$ ./rpn
```

## Commands
`p` Prints the top value of the stack.

`+` Pops two values off the stack, adds them and pushes the result on the stack.

`-` Pops two values off the stack, substracts the first from the second and pushes the result on the stack.

`*` Pops two values off the stack, multiplies them and pushes the result on the stack.

`/` Pops two values off the stack, divids the first from the second and pushes the result on the stack.

`%` Pops two values off the stack, divids the first from the second and pushes the remainder on the stack.

`^` Pops two values off the stack, use the first as the exponent and the second as the base and pushes the result on the stack.

`v` Pops one value, computes its square root, and pushes the result to the stack.

`d` Duplicates the top value of the stack.

`r` Swaps the two top values of the stack.

`c` Clears the stack.

`f` Prints the entire stack without modifying it.

## Limitations
This program is much more basic than dc and currently lacks some notable features like:
- decimal numbers
- memory registers
- conditionals and loops

## License
Copyright 2016-2023 Pierre Faivre. This is free software, and may be redistributed 
under the terms specified in the LICENSE file.
