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
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include "calc.h"
#include "stack.h"
#include "registers.h"
#include "io.h"

/**
 * Read and execute lines from a stream until EOF or a top-level q. A read
 * error (e.g. a directory passed as a script file, where getline fails) is
 * reported but does not stop the session.
 * @param s Stack to modify
 * @param regs Register file to modify
 * @param stream Stream to read from
 * @param name Name of the stream for error messages (a file path, or
 *        "standard input")
 * @return The stack after the parsed lines
 */
static Stack run_lines(Stack s, Registers regs, FILE *stream, const char *name) {
    char *line;
    while ((line = read_line_stream(stream)) != NULL) {
        s = calc_parse(s, regs, line);
        free(line);
        if (calc_quit_requested())  // q at the top level exits the program
            break;
    }
    if (ferror(stream))
        calc_report_errorf("could not read %s: %s", name, strerror(errno));
    return s;
}

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
    Registers regs = registers_init();

    // Option -e: evaluate the concatenated expressions as one input, at
    // macro depth 1 like dc's -e strings
    if (expression) {
        s = calc_parse_expression(s, regs, expression);
        free(expression);
    }

    // Script files, in order, one line at a time; an unreadable file is an
    // error but does not stop the session
    for (int i = optind; i < argc; i++) {
        if (calc_quit_requested())
            break;
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            calc_report_errorf("could not open file %s: %s", argv[i], strerror(errno));
            continue;
        }
        s = run_lines(s, regs, f, argv[i]);
        fclose(f);
    }

    // Standard input, one line per line: after script files (as in dc), or
    // when neither -e nor files were given
    if ((optind < argc || !expression) && !calc_quit_requested())
        s = run_lines(s, regs, stdin, "standard input");

    stack_delete(s);
    registers_delete(regs);
    return calc_had_error() ? 1 : 0;
}

