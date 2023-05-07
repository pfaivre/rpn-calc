/**
MIT License

Copyright (c) 2016-2023 Pierre Faivre

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

#include "io.h"

/**
 * Read the standard input until EOF or carriage return.
 * It can support an arbitrary amount of text.
 * TODO: Write directly in the input without the buffer
 */
char *bufferedInput() {
    char *input = calloc(1,1);
    char *realloc_ptr = NULL;
    char buffer[STDIN_BUFFER_SIZE] = {0};
    char c = 0;
    int count = 0;

    while(!feof(stdin) && c != '\n') {
        c = getchar();
        buffer[count++] = c;
        buffer[count] = '\0';

        if (strlen(buffer) >= STDIN_BUFFER_SIZE - 1) {
            realloc_ptr = realloc(input, strlen(input) + strlen(buffer) + 1);
            if (!realloc_ptr)
                return input;
            input = realloc_ptr;

            strcat(input, buffer);
            buffer[0] = '\0';
            count = 0;
        }
    }

    if (strlen(buffer) > 0) {
        realloc_ptr = realloc(input, strlen(input) + strlen(buffer) + 1);
        if (!realloc_ptr)
            return input;
        input = realloc_ptr;

        strcat(input, buffer);
    }

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
    // If not allocated yet, just copy
    if (dest == NULL) {
        dest = (char*) malloc(sizeof(char) * (strlen(src) + 1));
        
        if (dest == NULL) {
            fputs("Memory allocation error\n", stderr);
            exit(1);
        }
        
        strcpy(dest, src);
    }
    // Otherwise, concatenate them
    else {
        dest = (char*) realloc(dest, sizeof(char) * (strlen(src) + strlen(dest) + 1));
        
        if (dest == NULL) {
            fputs("Memory allocation error\n", stderr);
            exit(1);
        }

        strcat(dest, src);
    }

    return dest;
}

