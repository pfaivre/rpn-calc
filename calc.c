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
#include <math.h>

#include "calc.h"
#include "stack.h"

/**
 * Performs a single command
 */
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
        // Addition
        if (strcmp(command, "+") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            int a;
            s = stack_pop(s, &a);
            s->val += a;
        }
        // Subtraction
        else if (strcmp(command, "-") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            int a;
            s = stack_pop(s, &a);
            s->val -= a;
        }
        // Multiplication
        else if (strcmp(command, "*") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            int a;
            s = stack_pop(s, &a);
            s->val *= a;
        }
        // Division
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
        // Division remainder
        else if (strcmp(command, "%") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            if (s->val == 0) {
                puts("rpn: remainder by zero");
                return s;
            }

            int a;
            s = stack_pop(s, &a);
            s->val %= a;
        }
        // Exponentiation
        else if (strcmp(command, "^") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }

            int a, b;
            s = stack_pop(s, &a);
            s = stack_pop(s, &b);
            b = (int)round(pow(b, a));
            s = stack_push(s, b);
        }
        // Square root
        else if (strcmp(command, "v") == 0) {
            if (!stack_hasAtLeast(s, 1)) {
                puts("rpn: stack empty");
                return s;
            }

            int a;
            s = stack_pop(s, &a);
            a = (int)round(sqrt(a));
            s = stack_push(s, a);
        }
        // Clear the stack
        else if (strcmp(command, "c") == 0) {
            s = stack_delete(s);
        }
        // Print the top value
        else if (strcmp(command, "p") == 0) {
            if (!stack_hasAtLeast(s, 1)) {
                puts("rpn: stack empty");
                return s;
            }
            printf("%d\n", s->val);
        }
        // Print all the stack
        else if (strcmp(command, "f") == 0) {
            Stack below = s;
            while (below) {
                printf("%d\n", below->val);
                below = below->below;
            }
        }
        // Duplicate the value at the top
        else if (strcmp(command, "d") == 0) {
            if (!stack_hasAtLeast(s, 1)) {
                puts("rpn: stack empty");
                return s;
            }
            s = stack_push(s, s->val);
        }
        // Swap the top two value of the stack
        else if (strcmp(command, "r") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                puts("rpn: stack empty");
                return s;
            }
            int a, b;
            s = stack_pop(s, &a);
            s = stack_pop(s, &b);
            s = stack_push(s, a);
            s = stack_push(s, b);
        }
        // CTRL-D or end of file
        else if (command[0] == EOF) {
            // TODO: exit in the main to free the memory
            exit(0);
        }
        else {
            printf("rpn: %s unimplemented\n", command);
        }
    }

    return s;
}

/**
 * Performs a set of commands (e.g. "2 3 * p")
 */
Stack calc_parse(Stack s, const char* string) {
    // Perform a copy to the string to not alter the original one
    char *scopy = malloc((strlen(string) + 1) * sizeof(char));
    strcpy(scopy, string);

    // Split it with the spaces
    char *o = strtok(scopy, " \n");
    while (o != NULL) {
        // Comment
        if (o[0] == '#')
            break;

        s = _calc_proceed(s, o);

        o = strtok(NULL, " \n");
    }

    free(scopy);
    return s;
}

