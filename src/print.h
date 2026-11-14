/**
MIT License

Copyright (c) 2016-2026 Pierre Faivre

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef PRINT_H
#define PRINT_H

#include "stack.h"
#include "registers.h"

/** p: print the top value (a string is printed as-is) */
Stack print_cmd_top(Stack s, Registers regs, int depth);

/** n: pop the top value and print it without a trailing newline */
Stack print_cmd_nonl(Stack s, Registers regs, int depth);

/** P: pop the top value and print it without a trailing newline: a string
 * is printed as-is, a number as a string of bytes, most significant byte of
 * its absolute value first */
Stack print_cmd_bytes(Stack s, Registers regs, int depth);

/** f: print all the stack, one value per line */
Stack print_cmd_all(Stack s, Registers regs, int depth);

#endif
