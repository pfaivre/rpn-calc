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

#ifndef IO_H
#define IO_H

#include <stdio.h>

/**
 * Print a fatal error on stderr and exit with a non-zero status.
 * Reserved for unrecoverable failures (allocation failure, size overflow,
 * input too large): it exits immediately, intentionally without freeing
 * the calculator state — only main's exit_clean does that.
 */
[[noreturn]] void rpn_fatal(const char *msg);

[[nodiscard]] char *read_line(void);

/** Read one line from a stream (until newline or EOF), like read_line */
[[nodiscard]] char *read_line_stream(FILE *stream);

/** Open a script file for reading; NULL on failure (errno set) */
[[nodiscard]] FILE *script_open(const char *path);

/** Whether the standard input is a terminal (which already echoes input) */
bool stdin_is_tty(void);

/** Load the DC_LINE_LENGTH and DC_PROMPT environment variables (dc
 * compatibility); call once at startup, before any output */
void config_from_env(void);

/** Print the DC_PROMPT before an interactive read from the standard input;
 * nothing is printed when stdin is not a terminal, as in dc */
void stdin_prompt(void);

/** Read one line from the standard input, showing the prompt first when
 * stdin is a terminal and echoing the line when it is not (a terminal
 * already echoes what the user types) */
[[nodiscard]] char *read_stdin_line(void);

/** Forget the current wrap column (each printed number starts at column 0) */
void emit_reset_col(void);

/** Emit one character of a printed number: when the column reaches
 * DC_LINE_LENGTH - 1, a backslash-newline is emitted first and printing
 * continues on the next line; a newline itself resets the column */
void emit_char(int c);

/** Emit a NUL-terminated string of number characters (see emit_char) */
void emit_str(const char *s);

/** Emit raw bytes (a printed string, the byte stream of P on a number):
 * never wrapped, and not counted in the wrap column */
void emit_raw(const char *s, size_t n);

void print_usage(const char *progName);

void print_version(const char *version, const char *copyright);

#endif

