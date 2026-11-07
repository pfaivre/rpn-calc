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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gmp.h>

#include "calc.h"
#include "stack.h"

static bool _calc_errorFlag = false;

/**
 * Report an error on stderr and remember it, so main can exit non-zero
 * @param msg Error message (without the "rpn: " prefix and newline)
 */
static void _calc_error(const char *msg) {
    fprintf(stderr, "rpn: %s\n", msg);
    _calc_errorFlag = true;
}

/**
 * Whether any error was reported while parsing
 * @return true if at least one command failed
 */
bool calc_had_error() {
    return _calc_errorFlag;
}

typedef void (*MpzBinop)(mpz_t, const mpz_t, const mpz_t);

/**
 * Apply a binary mpz operation to the top two values of the stack
 * @param s Stack to modify
 * @param op The mpz operation to apply (e.g. mpz_add)
 * @return The top of the stack after the operation
 */
static Stack _calc_binaryOp(Stack s, MpzBinop op) {
    if (!stack_hasAtLeast(s, 2)) {
        _calc_error("stack underflow");
        return s;
    }
    mpz_t a;
    s = stack_pop(s, &a);
    op(s->val, s->val, a);
    mpz_clear(a);
    return s;
}

/**
 * Apply a division-like mpz operation, rejecting a zero divisor before
 * popping so the operands stay on the stack on error
 * @param s Stack to modify
 * @param op The mpz operation to apply (e.g. mpz_tdiv_q)
 * @param err Error message used when the divisor is zero
 * @return The top of the stack after the operation
 */
static Stack _calc_divOp(Stack s, MpzBinop op, const char *err) {
    if (!stack_hasAtLeast(s, 2)) {
        _calc_error("stack underflow");
        return s;
    }
    if (mpz_cmp_si(s->val, 0) == 0) {
        _calc_error(err);
        return s;
    }
    mpz_t a;
    s = stack_pop(s, &a);
    op(s->val, s->val, a);
    mpz_clear(a);
    return s;
}

/**
 * Performs a single command
 * @param s Stack to modify
 * @param command A single part of the input (e.g. an operand, an operator or
 * an instruction)
 * @return The top of the stack after performing the command
 */
Stack _calc_proceed(Stack s, const char* command) {
    // A number
    if (isdigit((unsigned char) command[0]) || command[0] == '_') {
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
        return s;
    }

    // Arithmetic operators
    if (strcmp(command, "+") == 0)
        return _calc_binaryOp(s, mpz_add);
    if (strcmp(command, "-") == 0)
        return _calc_binaryOp(s, mpz_sub);
    if (strcmp(command, "*") == 0)
        return _calc_binaryOp(s, mpz_mul);
    if (strcmp(command, "/") == 0)
        return _calc_divOp(s, mpz_tdiv_q, "divide by zero");
    if (strcmp(command, "%") == 0)
        return _calc_divOp(s, mpz_tdiv_r, "remainder by zero");

    // Division with remainder (dc-style ~ operator)
    if (strcmp(command, "~") == 0) {
        if (!stack_hasAtLeast(s, 2)) {
            _calc_error("stack underflow");
            return s;
        }
        if (mpz_cmp_si(s->val, 0) == 0) {
            _calc_error("divide by zero");
            return s;
        }
        mpz_t a, b, quotient, remainder;
        s = stack_pop(s, &a);  // Pop divisor (top of stack)
        s = stack_pop(s, &b);  // Pop dividend (next on stack)

        mpz_init(quotient);
        mpz_init(remainder);
        mpz_tdiv_qr(quotient, remainder, b, a);

        // Push results: quotient first, then remainder
        s = stack_push(s, quotient);
        s = stack_push(s, remainder);

        mpz_clear(a);
        mpz_clear(b);
        mpz_clear(quotient);
        mpz_clear(remainder);
        return s;
    }

    // Exponentiation
    if (strcmp(command, "^") == 0) {
        if (!stack_hasAtLeast(s, 2)) {
            _calc_error("stack underflow");
            return s;
        }
        if (mpz_sgn(s->val) < 0) {
            _calc_error("negative exponent");
            return s;
        }
        mpz_t a;
        s = stack_pop(s, &a);
        long exp = mpz_get_si(a);
        mpz_pow_ui(s->val, s->val, exp);
        mpz_clear(a);
        return s;
    }

    // Square root
    if (strcmp(command, "v") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        if (mpz_sgn(s->val) < 0) {
            _calc_error("square root of negative number");
            return s;
        }
        mpz_root(s->val, s->val, 2);
        return s;
    }

    // Clear the stack
    if (strcmp(command, "c") == 0)
        return stack_delete(s);

    // Print the top value
    if (strcmp(command, "p") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        mpz_out_str(stdout, 10, s->val);
        puts("");
        return s;
    }

    // Print all the stack
    if (strcmp(command, "f") == 0) {
        Stack below = s;
        while (below) {
            mpz_out_str(stdout, 10, below->val);
            puts("");
            below = below->below;
        }
        return s;
    }

    // Duplicate the value at the top
    if (strcmp(command, "d") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        return stack_push(s, s->val);
    }

    // Swap the top two values of the stack
    if (strcmp(command, "r") == 0) {
        if (!stack_hasAtLeast(s, 2)) {
            _calc_error("stack underflow");
            return s;
        }
        mpz_t tmp;
        mpz_init_set(tmp, s->val);
        mpz_set(s->val, s->below->val);
        mpz_set(s->below->val, tmp);
        mpz_clear(tmp);
        return s;
    }

    fprintf(stderr, "rpn: '%s' unimplemented\n", command);
    _calc_errorFlag = true;
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

    if (!token)
        return NULL;

    // End of input: clear the token so the caller's loop stops
    if (!str || *str == '\0') {
        token[0] = '\0';
        return NULL;
    }

    token[i] = '\0';

    // Ignore leading spaces, tabs and new lines
    while (*str == ' ' || *str == '\t' || *str == '\n')
        str++;

    // Copy token chars until the next one or end of string
    while (*str != ' ' && *str != '\t' && *str != '\n' && *str != '\0') {
        token[i++] = *str++;
        token[i] = '\0';

        // If the next char does not belong to the current token, stop copying
        if (_calc_tokenThreshold(token[i-1], *str))
            break;
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

    if (!token) {
        fputs("Memory allocation error\n", stderr);
        exit(1);
    }

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
