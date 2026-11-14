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

#define VERSION "0.2.2"
#define COPYRIGHT "Copyright 2016 Pierre Faivre"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#ifdef _WIN32
#include <fcntl.h>  // _O_BINARY
#include <io.h>     // _setmode, _fileno
#endif

#include "calc.h"
#include "stack.h"
#include "registers.h"
#include "io.h"

/**
 * Read and execute lines from a stream until EOF or a top-level q. A read
 * error (e.g. a directory passed as a script file, where the read fails) is
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
    while (true) {
        if (stream == stdin)
            stdin_prompt();  // interactive input: show the DC_PROMPT, as in dc
        line = read_line_stream(stream);
        if (line == NULL)
            break;
        s = calc_parse(s, regs, line);
        free(line);
        if (calc_quit_requested())  // q at the top level exits the program
            break;
    }
    if (ferror(stream))
        calc_report_errorf("could not read %s: %s", name, strerror(errno));
    return s;
}

/**
 * Read and execute a script file by name, one line at a time; the name "-"
 * reads the standard input. An unreadable file is an error but does not stop
 * the session.
 * @param s Stack to modify
 * @param regs Register file to modify
 * @param path File path, or "-" for standard input
 * @return The stack after the parsed lines
 */
static Stack run_file(Stack s, Registers regs, const char *path) {
    if (strcmp(path, "-") == 0)
        return run_lines(s, regs, stdin, "standard input");
    FILE *f = script_open(path);
    if (!f) {
        calc_report_errorf("could not open file %s: %s", path, strerror(errno));
        return s;
    }
    s = run_lines(s, regs, f, path);
    fclose(f);
    return s;
}

/**
 * Free the stack and register file, then exit with the given status. Every
 * exit path from main goes through here so no allocation is left behind.
 * @param s Stack to free
 * @param regs Register file to free
 * @param code Exit status
 */
[[noreturn]] static void exit_clean(Stack s, Registers regs, int code) {
    (void) stack_delete(s);  // the return value is always NULL
    registers_delete(regs);
    exit(code);
}

/** Long options for getopt_long */
static struct option long_options[] = {
    {"expression", required_argument, 0, 'e'},
    {"file",       required_argument, 0, 'f'},
    {"help",       no_argument,       0, 'h'},
    {"version",    no_argument,       0, 'V'},
    {0,            0,                 0,  0 }
};

int main(int argc, char **argv) {
#ifdef _WIN32
    // Piped output must stay byte-exact (text mode would translate LF to
    // CRLF); consoles keep text mode so output renders correctly. Piped
    // input goes binary so byte 0x1A is data, not EOF, as on POSIX; an
    // interactive console keeps text mode so Ctrl-Z still means EOF.
    if (!stdin_is_tty())
        _setmode(_fileno(stdin), _O_BINARY);
    if (!_isatty(_fileno(stdout)))
        _setmode(_fileno(stdout), _O_BINARY);
    if (!_isatty(_fileno(stderr)))
        _setmode(_fileno(stderr), _O_BINARY);
#endif
    config_from_env();  // DC_LINE_LENGTH and DC_PROMPT, before any output
    Stack s = stack_init();
    Registers regs = registers_init();
    bool saw_opt_script = false;  // whether any -e or -f option was given

    // Options are processed in command-line order, as in dc: each -e
    // expression runs one macro level above the top level (depth 1, like
    // dc's -e strings), each -f file is read and executed where it appears
    while (!calc_quit_requested()) {
        int option_index = 0;
        char option;

        option = getopt_long(argc, argv, "e:f:hV", long_options, &option_index);

        if (option == -1)
            break;

        switch (option) {
            case 'e':
                saw_opt_script = true;
                s = calc_parse_expression(s, regs, optarg);
                break;
            case 'f':
                saw_opt_script = true;
                s = run_file(s, regs, optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                exit_clean(s, regs, 0);
            case 'V':
                print_version(VERSION, COPYRIGHT);
                exit_clean(s, regs, 0);
            default:
                print_usage(argv[0]);
                exit_clean(s, regs, 1);
        }
    }

    // Positional script files, in order, after all options (as in dc)
    for (int i = optind; i < argc && !calc_quit_requested(); i++)
        s = run_file(s, regs, argv[i]);

    // Standard input, one line per line: after positional files, or when no
    // -e, -f or file argument was given at all (as in dc)
    bool read_stdin = optind < argc || !saw_opt_script;
    if (read_stdin && !calc_quit_requested())
        s = run_lines(s, regs, stdin, "standard input");

    exit_clean(s, regs, calc_had_error() ? 1 : 0);
}

