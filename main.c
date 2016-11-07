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

#define _POSIX_C_SOURCE 200809L // Enables use of strdup

#define VERSION "0.1"
#define COPYRIGHT "Copyright 2016 Pierre Faivre"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include "calc.h"
#include "stack.h"
#include "io.h"

/**
 * Things to be done next:
 * TODO: Support arbitrary precision for decimal
 * TODO: Read a script file
 * TODO: Add registers
 * TODO: Support compact notation limiting usage of space (e.g. "2 3*p")
 */


int main(int argc, char **argv) {
    bool opt_e = false;
    char *expression = NULL;
    char option;

    while (1) {
        int option_index = 0;
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
                opt_e = true;
                // TODO: Append rather than replace the string (for multiple use of -e)
                expression = strdup(optarg);
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

    // Option -e
    if (opt_e) {
        Stack s = stack_init();

        s = calc_parse(s, expression);

        free(expression);
        stack_delete(s);
    }
    // No -e nor -f
    else {
        char *input = NULL;
        Stack s = stack_init();
        input = bufferedInput();

        while (input && input[strlen(input)-1] != EOF) {
            s = calc_parse(s, input);

            free(input);

            input = bufferedInput();
        }

        stack_delete(s);
        free(input);
    }
    
    return 0;
}

