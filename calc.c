/**
MIT License

Copyright (c) 2016 Pierre Faivre

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
#include <ctype.h>

#include "calc.h"
#include "stack.h"

Stack _calc_proceed(Stack s, const char* command) {
    // A number
    if (isdigit(command[0]) || command[0] == '_') {
        int number;

        // Negative number
        if (command[0] == '_') {
            number = -strtol(command + 1, NULL, 10);
        }
        // Positive number
        else
            number = strtol(command, NULL, 10);

        s = stack_push(s, number);
    }
    // A command
    else {
        if (strcmp(command, "+") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            int a;
            s = stack_pop(s, &a);
            s->val += a;
        }
        else if (strcmp(command, "-") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            int a;
            s = stack_pop(s, &a);
            s->val -= a;
        }
        else if (strcmp(command, "*") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            int a;
            s = stack_pop(s, &a);
            s->val *= a;
        }
        else if (strcmp(command, "/") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            if (s->val == 0) {
                puts("rpn: divide by zero");
                return s;
            }

            int a;
            s = stack_pop(s, &a);
            s->val /= a;
        }
        else if (strcmp(command, "p") == 0) {
            if (!stack_hasAtLeast(s, 1)) {
                puts("rpn: stack empty");
                return s;
            }
            printf("%d\n", s->val);
        }
    }

    return s;
}

Stack calc_parse(Stack s, const char* string) {
    // Perform a copy to the string to not alter the original one
    char *scopy = malloc((strlen(string) + 1) * sizeof(char));
    strcpy(scopy, string);

    // Split it with the spaces
    char *o = strtok(scopy, " \n");
    while (o != NULL) {
        s = _calc_proceed(s, o);

        o = strtok(NULL, " \n");
    }

    free(scopy);
    return s;
}

void calc_printMessageCode(int code) {
    switch (code) {
    }
}
