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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gmp.h>

#include "calc.h"
#include "stack.h"

/**
 * Performs a single command
 * @param s Stack to modify
 * @param command A single part of the input (e.g. an operand, an operator or 
 * an instruction)
 * @return The top of the stack after performing the command
 */
Stack _calc_proceed(Stack s, const char* command) {
    // A number
    if (isdigit(command[0]) || command[0] == '_') {
        mpz_t number;

        // Negative number
        if (command[0] == '_') {
            mpz_init_set_str(number, command + 1, 10);
            mpz_neg(number, number);
        }
        // Positive number
        else
            mpz_init_set_str(number, command, 10);

        s = stack_push(s, number);
        mpz_clear(number);
    }

    // A command
    else {

        // Addition
        if (strcmp(command, "+") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            mpz_t a;
            s = stack_pop(s, &a);
            mpz_add(s->val, s->val, a);
            mpz_clear(a);
        }

        // Subtraction
        else if (strcmp(command, "-") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            mpz_t a;
            s = stack_pop(s, &a);
            mpz_sub(s->val, s->val, a);
            mpz_clear(a);
        }

        // Multiplication
        else if (strcmp(command, "*") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            mpz_t a;
            s = stack_pop(s, &a);
            mpz_mul(s->val, s->val, a);
            mpz_clear(a);
        }

        // Division
        else if (strcmp(command, "/") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            if (mpz_cmp_si(s->val, 0) == 0) {
                fputs("rpn: divide by zero\n", stderr);
                return s;
            }
            mpz_t a;
            s = stack_pop(s, &a);
            mpz_tdiv_q(s->val, s->val, a);
            mpz_clear(a);
        }

        // Division remainder
        else if (strcmp(command, "%") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            if (mpz_cmp_si(s->val, 0) == 0) {
                fputs("rpn: remainder by zero\n", stderr);
                return s;
            }
            mpz_t a;
            s = stack_pop(s, &a);
            mpz_tdiv_r(s->val, s->val, a);
            mpz_clear(a);
        }

        // Exponentiation
        else if (strcmp(command, "^") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            mpz_t a;
            s = stack_pop(s, &a);
            long exp = mpz_get_si(a);
            mpz_pow_ui(s->val, s->val, exp);
            mpz_clear(a);
        }

        // Square root
        else if (strcmp(command, "v") == 0) {
            if (!stack_hasAtLeast(s, 1)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            mpz_root(s->val, s->val, 2);
        }

        // Clear the stack
        else if (strcmp(command, "c") == 0) {
            s = stack_delete(s);
        }

        // Print the top value
        else if (strcmp(command, "p") == 0) {
            if (!stack_hasAtLeast(s, 1)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            mpz_out_str(stdout, 10, s->val);
            puts("");
        }

        // Print all the stack
        else if (strcmp(command, "f") == 0) {
            Stack below = s;
            while (below) {
                mpz_out_str(stdout, 10, below->val);
                puts("");
                below = below->below;
            }
        }

        // Duplicate the value at the top
        else if (strcmp(command, "d") == 0) {
            if (!stack_hasAtLeast(s, 1)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            s = stack_push(s, s->val);
        }

        // Swap the top two value of the stack
        else if (strcmp(command, "r") == 0) {
            if (!stack_hasAtLeast(s, 2)) {
                fputs("rpn: stack empty\n", stderr);
                return s;
            }
            mpz_t tmp;
            mpz_init_set(tmp, s->val);
            mpz_set(s->val, s->below->val);
            mpz_set(s->below->val, tmp);
            mpz_clear(tmp);
        }
        else {
            printf("rpn: '%c' (%#o) unimplemented\n", command[0], command[0]);
        }
    }

    return s;
}

/**
 * Detects if the two chars belong to different commands
 */
bool _calc_tokenThreshold(char c1, char c2) {
    // digit or minus sign (_) followed by an other digit 
    if ((c1 == '_' || (c1 >= '0' && c1 <= '9'))
        && (c2 >= '0' && c2 <= '9'))
        return false;

    return true;
}

/**
 * Find the first command on the string
 * @param token An allocated string to store the token
 * @param str String to search the element on (operator, operand or command)
 * @return Pointer to str after the extracted token
 */
char *_calc_firstToken(char *token, char *str) {
    int i = 0;
    token[i] = '\0';

    if (!token || !str || *str == '\0') {
        *token = '\0';
        return NULL;
    }

    // Ignore trailing spaces and new lines    
    while(*str == ' ' || *str == '\n') {
        if (*str == '\0')
            return NULL;

        str++;
    }

    // Copy token chars until next one or end of string
    while(*str != ' ' && *str != '\n' && *str != '\0') {
        token[i++] = *str++;
        token[i] = '\0';
        
        // If the next char does not belong to the current token, stop copying
        if (_calc_tokenThreshold(token[i-1], *str)) {
            break;
        }
    }

    return str;
}

/**
 * Performs a set of commands (e.g. "2 3*p")
 * @param s Stack to modify
 * @param string Commands to execute
 * @return The top of the stack after performing the commands
 */
Stack calc_parse(Stack s, char* string) {
    char *ptr = string;
    char *token = calloc((strlen(string) + 1), sizeof(char));

    ptr = _calc_firstToken(token, ptr);

    while (token && token[0] != '\0') {
        // Comment
        if (token[0] == '#')
            break;

        s = _calc_proceed(s, token);

        ptr = _calc_firstToken(token, ptr);
    }

    free(token);
    return s;
}

