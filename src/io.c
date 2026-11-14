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
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdckdint.h>

#ifdef _WIN32
#include <io.h>  // _isatty, _fileno
#else
#include <unistd.h>  // isatty, fileno
#endif

#include "io.h"

/** Default wrap width of printed numbers, as in dc */
#define DEFAULT_LINE_LENGTH 70L

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

static long _out_line_length = DEFAULT_LINE_LENGTH;
static const char *_out_prompt = "";
static long _out_col = 0;

/**
 * Load the DC_LINE_LENGTH and DC_PROMPT environment variables, parsing
 * DC_LINE_LENGTH exactly like dc (see README). Call once at startup, before
 * any output. Invalid values are silently ignored, keeping the default.
 */
void config_from_env(void) {
    const char *e = getenv("DC_LINE_LENGTH");
    if (e != NULL) {
        // Like dc: strtol with base 0 (so 012 is octal 10 and 0x10 is 16),
        // with optional leading whitespace and sign; only whitespace may
        // follow the number, so an empty or all-whitespace value is 0. The
        // value must be 0 — which disables wrapping — or in [2, INT_MAX].
        errno = 0;
        char *end = NULL;
        long v = strtol(e, &end, 0);
        bool valid = errno != ERANGE && (v == 0 || (v >= 2 && v <= INT_MAX));
        for (const char *p = end; valid && *p != '\0'; p++) {
            if (!isspace((unsigned char) *p))
                valid = false;
        }
        if (valid)
            _out_line_length = v;
        // invalid values (1, negatives, junk, out of int range, overflow)
        // are silently ignored, keeping the default, as in dc
    }
    const char *p = getenv("DC_PROMPT");
    if (p != NULL)
        _out_prompt = p;  // the environment string lives for the whole process
}

/**
 * Print the DC_PROMPT before an interactive read from the standard input.
 * Nothing is printed when stdin is not a terminal (piped input), as in dc.
 */
void stdin_prompt(void) {
    if (stdin_is_tty()) {
        fputs(_out_prompt, stdout);
        fflush(stdout);
    }
}

/**
 * Read one line from the standard input: show the DC_PROMPT first when stdin
 * is a terminal, and echo the line when it is not (a terminal already echoes
 * what the user types).
 * @return A newly allocated string, or NULL at EOF (see read_line)
 */
[[nodiscard]] char *read_stdin_line(void) {
    stdin_prompt();
    char *line = read_line();
    if (line != NULL && !stdin_is_tty()) {
        emit_raw(line, strlen(line));
        emit_char('\n');
    }
    return line;
}

/**
 * Forget the current wrap column: each printed number starts wrapping from
 * column 0, as in dc.
 */
void emit_reset_col(void) {
    _out_col = 0;
}

/**
 * Emit one character of a printed number: when the column reaches
 * _out_line_length - 1, a backslash-newline is emitted first and printing
 * continues on the next line; a newline itself resets the column. A line
 * length of 0 or 1 disables wrapping.
 * @param c The character to emit
 */
void emit_char(int c) {
    if (c == '\n') {
        putchar('\n');
        _out_col = 0;
        return;
    }
    if (_out_line_length > 1 && _out_col == _out_line_length - 1) {
        fputs("\\\n", stdout);  // wrap: the backslash is the last column
        _out_col = 0;
    }
    putchar(c);
    _out_col++;
}

/**
 * Emit a NUL-terminated string of number characters, wrapping like emit_char
 * @param s The string to emit
 */
void emit_str(const char *s) {
    for (; *s != '\0'; s++)
        emit_char(*s);
}

/**
 * Emit raw bytes (a printed string, the byte stream of P on a number):
 * never wrapped, and not counted in the wrap column
 * @param s The bytes to emit
 * @param n Their count
 */
void emit_raw(const char *s, size_t n) {
    fwrite(s, 1, n, stdout);
}

void print_usage(const char *progName) {
    printf("Usage: %s [OPTION] [file...]\n", progName);
    puts("  -e, --expression=EXPR    evaluate expression");
    puts("  -f, --file=FILE          evaluate contents of file");
    puts("  -h, --help               display this help and exit");
    puts("  -V, --version            output version information and exit");
    puts("  file...                  read and execute script files, in order");
    puts("");
    puts("Environment variables:");
    puts("  DC_LINE_LENGTH  wrap column of printed numbers: a backslash-newline is");
    puts("                  emitted at that column (default 70; 0 disables wrapping);");
    puts("                  parsed like dc: decimal, octal or hex, 0 or 2..2147483647");
    puts("  DC_PROMPT       prompt printed before each read from a terminal");
    puts("                  (default: no prompt)");
}

void print_version(const char *version, const char *copyright) {
    printf("rpn %s\n", version);
    puts("");
    puts(copyright);
    puts("This is free software; see the source for copying conditions.  There is NO");
    puts("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,");
    puts("to the extent permitted by law.");
}

