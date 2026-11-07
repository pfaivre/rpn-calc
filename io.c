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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdckdint.h>

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
 * Append count bytes to a growing NUL-terminated string, checking the new
 * size against overflow
 * @param input The string (allocated); may be reallocated
 * @param input_len Current length of input in bytes; updated on return
 * @param buf Bytes to append
 * @param count Number of bytes to append
 * @return The (possibly moved) string
 */
static char *append_bytes(char *input, size_t *input_len, const char *buf, size_t count) {
    size_t size;
    if (ckd_add(&size, *input_len, count) || ckd_add(&size, size, (size_t) 1))
        rpn_fatal("input too large");

    char *grown = realloc(input, size);
    if (!grown)
        rpn_fatal("memory allocation error");

    memcpy(grown + *input_len, buf, count);
    *input_len += count;
    grown[*input_len] = '\0';
    return grown;
}

/**
 * Read one line from the standard input (until newline or EOF).
 * It can support an arbitrary amount of text.
 * TODO: Write directly in the input without the buffer
 * @return A newly allocated string holding the line without its trailing
 *         newline, an empty string for an empty line, or NULL at EOF
 */
char *bufferedInput() {
    char *input = calloc(1,1);
    if (!input)
        rpn_fatal("memory allocation error");

    char buffer[STDIN_BUFFER_SIZE];
    int c;
    size_t count = 0;
    size_t input_len = 0;

    c = getchar();
    if (c == EOF) {
        free(input);
        return NULL;
    }

    while (c != EOF && c != '\n') {
        buffer[count++] = (char) c;

        // Flush by byte count: strlen() stops at a NUL byte in the input and
        // would let count grow past the end of buffer
        if (count >= (size_t) STDIN_BUFFER_SIZE - 1) {
            input = append_bytes(input, &input_len, buffer, count);
            count = 0;
        }

        c = getchar();
    }

    if (count > 0)
        input = append_bytes(input, &input_len, buffer, count);

    return input;
}

void printUsage(const char *progName) {
    printf("Usage: %s [OPTION]\n", progName);
    puts("  -e, --expression=EXPR    evaluate expression");
    puts("  -h, --help               display this help and exit");
    puts("  -V, --version            output version information and exit");
}

void printVersion(const char *version, const char *copyright) {
    printf("rpn %s\n", version);
    puts("");
    puts(copyright);
    puts("This is free software; see the source for copying conditions.  There is NO");
    puts("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,");
    puts("to the extent permitted by law.");
}

/**
 * Allocate the space to concatenate two strings and copy src at the end of dest.
 * @param dest String to modify
 * @param src String to append to dest
 * @return The new pointer location
 */
char *strAppend(char *dest, const char *src) {
    size_t src_len = strlen(src);
    size_t size;

    // If not allocated yet, just copy
    if (dest == NULL) {
        if (ckd_add(&size, src_len, (size_t) 1))
            rpn_fatal("expression too large");

        dest = malloc(size);
        if (!dest)
            rpn_fatal("memory allocation error");

        memcpy(dest, src, size);  // copies the terminating NUL as well
    }
    // Otherwise, concatenate them
    else {
        size_t dest_len = strlen(dest);
        if (ckd_add(&size, dest_len, src_len) || ckd_add(&size, size, (size_t) 1))
            rpn_fatal("expression too large");

        dest = realloc(dest, size);
        if (!dest)
            rpn_fatal("memory allocation error");

        memcpy(dest + dest_len, src, src_len + 1);  // copies the terminating NUL as well
    }

    return dest;
}

