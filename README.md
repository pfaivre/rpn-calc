# rpn-calc [![Build Status](https://travis-ci.org/pfaivre/rpn-calc.svg?branch=master)](https://travis-ci.org/pfaivre/rpn-calc)
A humble dc clone

This is a calculator that uses the reverse-polish notation (RPN).
It stores the numbers in a stack and operates with the top ones until the desired result is reached.

For example the calculation `(3 + 5) * 7` can be noted as `3 5 + 7 *` in RPN.

## Usage

By default, it takes the commands in the stdin so it can be used either interactively or with a pipe
```sh
$ rpn
```

```sh
$ echo "2 3 + p" | rpn
```

