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

#define VERSION "0.1"
#define COPYRIGHT "Copyright 2016 Pierre Faivre"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "calc.h"
#include "stack.h"
#include "io.h"

/**
 * Things to be done next:
 * TODO: Read a script file
 * TODO: Support arbitrary input/output radices
 * TODO: Support registers
 * TODO: Support macros
 * TODO: Support conditionals
 * TODO: Support arbitrary precision for decimal
 */


int main(int argc, char **argv) {
    char *expression = NULL;

    while (1) {
        int option_index = 0;
        char option;
        static struct option long_options[] = {
            {"expression", required_argument, 0, 'e'},
            {"help",       no_argument,       0, 'h'},
            {"version",    no_argument,       0, 'V'},
            {0,            0,                 0,  0 }
        };

        option = getopt_long(argc, argv, "e:hV", long_options, &option_index);

        if (option == -1)
            break;

        switch (option) {
            case 'e':
                expression = strAppend(expression, " "); // in case of multiple -e
                expression = strAppend(expression, optarg);
                break;
            case 'h':
                printUsage(argv[0]);
                exit(0);
            case 'V':
                printVersion(VERSION, COPYRIGHT);
                exit(0);
            default:
                printUsage(argv[0]);
                exit(1);
        }
    }

    Stack s = stack_init();

    // Option -e: evaluate the concatenated expressions as one input
    if (expression) {
        s = calc_parse(s, expression);
        free(expression);
    }
    // Otherwise read expressions from the standard input, one per line
    else {
        char *line;
        while ((line = read_line()) != NULL) {
            s = calc_parse(s, line);
            free(line);
        }
    }

    stack_delete(s);
    return calc_had_error() ? 1 : 0;
}

