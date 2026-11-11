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

/* getline() is POSIX, not ISO C23: request it explicitly so it is declared
 * under -std=c23 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "io.h"

/**
 * Print a fatal error on stderr and exit with a non-zero status
 * @param msg Error message (without the "rpn: " prefix and newline)
 */
[[noreturn]] void rpn_fatal(const char *msg) {
    fprintf(stderr, "rpn: %s\n", msg);
    exit(1);
}

/**
 * Read one line from a stream (until newline or EOF).
 * The line can have arbitrary length and may contain NUL bytes; the
 * parser only sees up to the first NUL.
 * @param stream The stream to read from
 * @return A newly allocated string holding the line without its trailing
 *         newline, an empty string for an empty line, or NULL at EOF
 */
char *read_line_stream(FILE *stream) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t len = getline(&line, &cap, stream);

    if (len < 0) {
        free(line);
        return NULL;
    }
    if (len > 0 && line[len - 1] == '\n')
        line[--len] = '\0';
    return line;
}

/**
 * Read one line from the standard input
 * @return A newly allocated string, or NULL at EOF (see read_line_stream)
 */
char *read_line(void) {
    return read_line_stream(stdin);
}

/**
 * Open a script file for reading
 * @param path File path
 * @return The stream, or NULL on failure (errno set)
 */
FILE *script_open(const char *path) {
    return fopen(path, "r");
}

/**
 * Whether the standard input is a terminal (which already echoes what the
 * user types, so no explicit echo is needed)
 * @return true when stdin is a tty
 */
bool stdin_is_tty(void) {
    return isatty(fileno(stdin));
}

void printUsage(const char *progName) {
    printf("Usage: %s [OPTION] [file...]\n", progName);
    puts("  -e, --expression=EXPR    evaluate expression");
    puts("  -f, --file=FILE          evaluate contents of file");
    puts("  -h, --help               display this help and exit");
    puts("  -V, --version            output version information and exit");
    puts("  file...                  read and execute script files, in order");
}

void printVersion(const char *version, const char *copyright) {
    printf("rpn %s\n", version);
    puts("");
    puts(copyright);
    puts("This is free software; see the source for copying conditions.  There is NO");
    puts("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,");
    puts("to the extent permitted by law.");
}

