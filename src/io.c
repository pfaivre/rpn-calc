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

#ifndef _WIN32
/* isatty() and fileno() are POSIX, not ISO C23: request them explicitly so
 * they are declared under -std=c23 */
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdckdint.h>

#ifdef _WIN32
#include <io.h>  // _isatty, _fileno
#else
#include <unistd.h>  // isatty, fileno
#endif

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
 * Portable replacement for getline(), which Windows does not provide.
 * On Windows the stream is read in binary mode (see script_open/main), so a
 * trailing CR of a CRLF line ending is stripped here, and a console line
 * holding only Ctrl+D (0x04) counts as EOF.
 * @param stream The stream to read from
 * @return A newly allocated string holding the line without its trailing
 *         newline, an empty string for an empty line, or NULL at EOF or on
 *         a read error (the caller reports it via ferror)
 */
char *read_line_stream(FILE *stream) {
    size_t cap = 128;
    size_t len = 0;
    char *line = malloc(cap);
    if (!line)
        rpn_fatal("out of memory");
    int c;
    while ((c = fgetc(stream)) != EOF && c != '\n') {
        if (len + 1 == cap) {  // always keep room for the terminating NUL
            size_t new_cap;
            if (ckd_mul(&new_cap, cap, 2))
                rpn_fatal("line too long");
            char *new_line = realloc(line, new_cap);
            if (!new_line)
                rpn_fatal("out of memory");
            line = new_line;
            cap = new_cap;
        }
        line[len++] = (char) c;
    }
    if (c == EOF && (len == 0 || ferror(stream))) {
        free(line);  // like getline, a read error discards the partial line
        return NULL;
    }
#ifdef _WIN32
    if (len > 0 && line[len - 1] == '\r')  // strip the CR of a CRLF ending
        len--;
    // On a Windows console in text mode Ctrl+Z (0x1A) is already EOF, but
    // Ctrl+D arrives as byte 0x04; treat a line that is only that byte as
    // EOF, like Ctrl+D on a POSIX terminal (piped input keeps 0x04 as data)
    if (stream == stdin && stdin_is_tty() && len == 1 && line[0] == '\x04') {
        free(line);
        return NULL;
    }
#endif
    line[len] = '\0';
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
 * Open a script file for reading, in binary mode: on Windows text mode would
 * treat byte 0x1A as EOF; CRLF line endings are handled by read_line_stream.
 * ("rb" is identical to "r" on POSIX.)
 * @param path File path
 * @return The stream, or NULL on failure (errno set)
 */
FILE *script_open(const char *path) {
    return fopen(path, "rb");
}

/**
 * Whether the standard input is a terminal (which already echoes what the
 * user types, so no explicit echo is needed)
 * @return true when stdin is a tty
 */
bool stdin_is_tty(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdin));
#else
    return isatty(fileno(stdin));
#endif
}

void print_usage(const char *progName) {
    printf("Usage: %s [OPTION] [file...]\n", progName);
    puts("  -e, --expression=EXPR    evaluate expression");
    puts("  -f, --file=FILE          evaluate contents of file");
    puts("  -h, --help               display this help and exit");
    puts("  -V, --version            output version information and exit");
    puts("  file...                  read and execute script files, in order");
}

void print_version(const char *version, const char *copyright) {
    printf("rpn %s\n", version);
    puts("");
    puts(copyright);
    puts("This is free software; see the source for copying conditions.  There is NO");
    puts("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,");
    puts("to the extent permitted by law.");
}

