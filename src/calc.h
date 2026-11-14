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

#ifndef CALC_H
#define CALC_H

#include "stack.h"
#include "registers.h"

[[nodiscard]] Stack calc_parse(Stack s, Registers regs, char* string);

/** Parse a -e command-line expression (at macro depth 1, as dc executes -e strings) */
[[nodiscard]] Stack calc_parse_expression(Stack s, Registers regs, char* string);

[[nodiscard]] bool calc_had_error();

/** Report an error outside the parser (e.g. an unreadable script file):
 * prints "rpn: ..." to stderr and sets the error flag for the exit status */
void calc_report_errorf(const char *fmt, ...);

/** Whether q asked to leave the top-level input (main then stops and exits) */
[[nodiscard]] bool calc_quit_requested();

/** The output base set by o (2..16), used when printing numbers */
[[nodiscard]] long calc_out_base(void);

#endif

