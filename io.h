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

void printUsage(const char *progName);

void printVersion(const char *version, const char *copyright);

#endif

