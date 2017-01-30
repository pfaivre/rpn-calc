# rpn-calc [![Build Status](https://travis-ci.org/pfaivre/rpn-calc.svg?branch=master)](https://travis-ci.org/pfaivre/rpn-calc)
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

## License
Copyright 2016 Pierre Faivre. This is free software, and may be redistributed 
under the terms specified in the LICENSE file.
