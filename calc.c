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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdckdint.h>

#include <gmp.h>

#include "calc.h"
#include "io.h"
#include "stack.h"

/* Maximum size of a ^ result, in bits (32 MiB). Beyond this, mpz_pow_ui
 * risks exhausting memory and GMP aborts on allocation failure, so the
 * exponent is rejected up front with a graceful error instead. */
#define MAX_POW_BITS ((size_t) 1 << 28)

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
static Stack _calc_binary_op(Stack s, MpzBinop op) {
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
static Stack _calc_div_op(Stack s, MpzBinop op, const char *err) {
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
static Stack _calc_proceed(Stack s, const char* command) {
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
        return _calc_binary_op(s, mpz_add);
    if (strcmp(command, "-") == 0)
        return _calc_binary_op(s, mpz_sub);
    if (strcmp(command, "*") == 0)
        return _calc_binary_op(s, mpz_mul);
    if (strcmp(command, "/") == 0)
        return _calc_div_op(s, mpz_tdiv_q, "divide by zero");
    if (strcmp(command, "%") == 0)
        return _calc_div_op(s, mpz_tdiv_r, "remainder by zero");

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
        // A positive exponent that does not fit in an unsigned long cannot
        // be computed; reject it before popping so the operands stay on the
        // stack (dc reports "exponent too large in raise")
        if (mpz_sgn(s->val) > 0 && !mpz_fits_ulong_p(s->val)) {
            _calc_error("exponent too large in raise");
            return s;
        }
        // Cap the result size: base^exp has about exp * bits(base) bits, and
        // beyond MAX_POW_BITS GMP would attempt a huge allocation and abort
        // on failure. Bases 0 and +-1 are exact for any exponent and stay
        // instant. Checked before popping so the operands stay on the stack.
        if (mpz_sgn(s->val) > 0 && mpz_cmpabs_ui(s->below->val, 1) > 0) {
            size_t result_bits;
            if (ckd_mul(&result_bits, (size_t) mpz_get_ui(s->val),
                        mpz_sizeinbase(s->below->val, 2))
                || result_bits > MAX_POW_BITS) {
                _calc_error("exponent too large in raise");
                return s;
            }
        }
        mpz_t a;
        s = stack_pop(s, &a);
        if (mpz_sgn(a) < 0) {
            // Negative exponent
            // at the default scale 0 that is the truncation toward zero of
            // the rational result, i.e. 0 unless the base is 1 or _1
            if (mpz_cmp_si(s->val, 1) == 0)
                mpz_set_ui(s->val, 1);
            else if (mpz_cmp_si(s->val, -1) == 0)
                mpz_set_si(s->val, mpz_odd_p(a) ? -1 : 1);
            else
                mpz_set_ui(s->val, 0);
        } else {
            mpz_pow_ui(s->val, s->val, mpz_get_ui(a));
        }
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

    // Pop the top value and print it without a trailing newline
    if (strcmp(command, "n") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        mpz_out_str(stdout, 10, s->val);
        mpz_t a;
        s = stack_pop(s, &a);
        mpz_clear(a);
        return s;
    }

    // Pop the top value and print it as a string of bytes, most significant
    // byte of its absolute value first, without a trailing newline (dc's P
    // on a number; string literals are not supported)
    if (strcmp(command, "P") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        mpz_t a;
        s = stack_pop(s, &a);
        mpz_abs(a, a);
        size_t nbytes = mpz_sizeinbase(a, 256);  // at least 1, even for zero
        unsigned char *bytes = malloc(nbytes);
        if (!bytes)
            rpn_fatal("memory allocation error");
        size_t count = 0;
        mpz_export(bytes, &count, 1, 1, 1, 0, a);
        if (count == 0)
            putchar(0);  // zero prints as a single NUL byte, as in dc
        else
            fwrite(bytes, 1, count, stdout);
        free(bytes);
        mpz_clear(a);
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

    // Swap the top two values of the stack (a no-op with fewer than two
    // values, as in dc)
    if (strcmp(command, "r") == 0) {
        if (!stack_hasAtLeast(s, 2))
            return s;
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
static bool _calc_tokenThreshold(char c1, char c2) {
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
static char *_calc_firstToken(char *token, char *str) {
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
    size_t tokcap;
    if (ckd_add(&tokcap, strlen(string), (size_t) 1))
        rpn_fatal("input too large");

    char *token = calloc(tokcap, sizeof(char));
    if (!token)
        rpn_fatal("memory allocation error");

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
