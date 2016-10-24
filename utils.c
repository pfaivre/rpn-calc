#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

/**
 * Read the standard input until the return key is pressed.
 * It can support an arbitrary amount of text.
 * TODO: Write directly in the input without the buffer
 */
char *bufferedInput() {
	char *input = calloc(1,1);
    char *realloc_ptr = NULL;
	char buffer[STDIN_BUFFER_SIZE];
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

