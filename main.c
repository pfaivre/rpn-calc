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

#include "calc.h"
#include "stack.h"
#include "io.h"

/**
 * Things to be done next:
 * TODO: Support arbitrary precision for decimal
 * TODO: Read a script file
 * TODO: Read expression from -e parameter
 * TODO: Add registers
 * TODO: Support compact notation limiting usage of space (e.g. "2 3*p")
 */


int main(int argc, char** argv) {
    char* input = NULL;
    Stack s = stack_init();

    input = bufferedInput();

    while (input && input[strlen(input)-1] != EOF) {
        s = calc_parse(s, input);

        free(input);

        input = bufferedInput();
    }

    stack_delete(s);
    free(input);
    puts("");

    return 0;
}

