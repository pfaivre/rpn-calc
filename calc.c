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
#include <stdarg.h>
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

/* Maximum nesting depth of macro execution (x). A deeper nesting is
 * rejected with an error instead of overflowing the C stack. */
#define MAX_MACRO_DEPTH 1000

static bool _calc_errorFlag = false;

/* Levels of macro execution still to unwind, set by q/Q: when a frame of
 * _calc_parse_depth sees it positive, it abandons the rest of its input */
static int _calc_quit = 0;

/* Set by q when issued at the top level or in a macro called directly from
 * the top level: the whole program exits in that case */
static bool _calc_exitAll = false;

/**
 * Report a formatted error on stderr and remember it, so main can exit
 * non-zero
 * @param fmt printf-style format (the "rpn: " prefix and newline are added)
 */
static void _calc_errorf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fputs("rpn: ", stderr);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    _calc_errorFlag = true;
}

/**
 * Report an error on stderr and remember it, so main can exit non-zero
 * @param msg Error message (without the "rpn: " prefix and newline)
 */
static void _calc_error(const char *msg) {
    _calc_errorf("%s", msg);
}

/**
 * Whether any error was reported while parsing
 * @return true if at least one command failed
 */
bool calc_had_error() {
    return _calc_errorFlag;
}

/**
 * Whether q asked to leave the top-level input: main stops reading lines
 * and exits when this is true after a top-level parse
 * @return true if q/Q unwound past the top level
 */
bool calc_quit_requested() {
    return _calc_quit > 0 || _calc_exitAll;
}

typedef void (*MpzBinop)(mpz_t, const mpz_t, const mpz_t);

/**
 * Whether the top two values of the stack are both numbers
 * @param s Stack to inspect
 * @return true when both the top and the second values are numbers
 */
static bool _calc_top_two_numeric(Stack s) {
    return stack_top(s)->kind == VAL_NUM && stack_second(s)->kind == VAL_NUM;
}

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
    if (!_calc_top_two_numeric(s)) {
        _calc_error("non-numeric value");
        return s;
    }
    Value a;
    s = stack_pop(s, &a);
    op(stack_top(s)->num, stack_top(s)->num, a.num);
    value_clear(&a);
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
    if (!_calc_top_two_numeric(s)) {
        _calc_error("non-numeric value");
        return s;
    }
    if (mpz_cmp_si(stack_top(s)->num, 0) == 0) {
        _calc_error(err);
        return s;
    }
    Value a;
    s = stack_pop(s, &a);
    op(stack_top(s)->num, stack_top(s)->num, a.num);
    value_clear(&a);
    return s;
}

/**
 * Print a value in decimal: a string as-is, a number in base 10
 * @param v Value to print
 * @param newline Whether to append a trailing newline
 */
static void _calc_print(const Value *v, bool newline) {
    if (v->kind == VAL_STR) {
        fputs(v->str, stdout);
        if (newline)
            putchar('\n');
    } else {
        mpz_out_str(stdout, 10, v->num);
        if (newline)
            puts("");
    }
}

/**
 * Print a number as a string of bytes, most significant byte of its
 * absolute value first; zero prints as a single NUL byte
 * @param num Number to print
 */
static void _calc_print_bytes(const mpz_t num) {
    mpz_t abs;
    mpz_init(abs);
    mpz_abs(abs, num);
    size_t nbytes = mpz_sizeinbase(abs, 256);  // at least 1, even for zero
    unsigned char *bytes = malloc(nbytes);
    if (!bytes)
        rpn_fatal("memory allocation error");
    size_t count = 0;
    mpz_export(bytes, &count, 1, 1, 1, 0, abs);
    if (count == 0)
        putchar(0);  // zero prints as a single NUL byte
    else
        fwrite(bytes, 1, count, stdout);
    free(bytes);
    mpz_clear(abs);
}

static Stack _calc_parse_depth(Stack s, Registers regs, char *string, int depth);

/**
 * Whether a value is usable as an array index: a nonnegative integer
 * @param v Value to inspect
 * @return true when v is a number greater than or equal to 0
 */
static bool _calc_is_index(const Value *v) {
    return v->kind == VAL_NUM && mpz_sgn(v->num) >= 0;
}

/**
 * Push the number 0 onto the stack (the load of an empty register or of a
 * never-stored array cell)
 * @param s Stack to modify
 * @return The stack with 0 on top
 */
static Stack _calc_push_zero(Stack s) {
    mpz_t zero;
    mpz_init_set_ui(zero, 0);
    s = stack_push_num(s, zero);
    mpz_clear(zero);
    return s;
}

/**
 * Performs a register command (s, l, S, L, : or ;), whose register name is
 * given separately (it is the token following the command letter)
 * @param s Stack to modify
 * @param regs Register file to modify
 * @param cmd The command letter (one of s, l, S, L, : or ;)
 * @param name The register name (one byte)
 * @return The stack after performing the command
 */
static Stack _calc_register_cmd(Stack s, Registers regs, char cmd, unsigned char name) {
    switch (cmd) {
        // s: pop the top of the main stack and make it the register's only
        // value (replacing any previous content)
        case 's': {
            if (!stack_hasAtLeast(s, 1)) {
                _calc_error("stack underflow");
                return s;
            }
            Stack rs = register_stack(regs, name);
            rs = stack_clear(rs);
            stack_move_top(s, rs);
            return s;
        }
        // l: push a copy of the register's top value; an empty register
        // loads 0
        case 'l': {
            Stack rs = register_stack(regs, name);
            if (stack_hasAtLeast(rs, 1))
                return stack_push_value(s, *stack_top(rs));
            return _calc_push_zero(s);
        }
        // S: pop the top of the main stack and push it onto the register's
        // own stack
        case 'S': {
            if (!stack_hasAtLeast(s, 1)) {
                _calc_error("stack underflow");
                return s;
            }
            stack_move_top(s, register_stack(regs, name));
            return s;
        }
        // L: pop the register's stack top and push it onto the main stack
        case 'L': {
            Stack rs = register_stack(regs, name);
            if (!stack_hasAtLeast(rs, 1)) {
                _calc_errorf("stack register '%c' is empty", name);
                return s;
            }
            stack_move_top(rs, s);
            return s;
        }
        // : array store: the top of the main stack is the index, the second
        // is the value stored at that index (the value may be a string)
        case ':': {
            if (!stack_hasAtLeast(s, 2)) {
                _calc_error("stack underflow");
                return s;
            }
            if (!_calc_is_index(stack_top(s))) {
                _calc_error("array index must be a nonnegative integer");
                return s;
            }
            Value index, val;
            s = stack_pop(s, &index);  // Pop the index (top of stack)
            s = stack_pop(s, &val);    // Pop the value
            register_array_store(regs, name, index.num, val);  // moves val
            value_clear(&index);
            return s;
        }
        // ; array load: pop the index; push the stored value, or 0 when the
        // index was never stored
        case ';': {
            if (!stack_hasAtLeast(s, 1)) {
                _calc_error("stack underflow");
                return s;
            }
            if (!_calc_is_index(stack_top(s))) {
                _calc_error("array index must be a nonnegative integer");
                return s;
            }
            Value index;
            s = stack_pop(s, &index);
            const Value *cell = register_array_find(regs, name, index.num);
            if (cell)
                s = stack_push_value(s, *cell);
            else
                s = _calc_push_zero(s);
            value_clear(&index);
            return s;
        }
    }
    return s;
}

/**
 * Execute the top value of the stack as a macro (the core of x): a
 * string is popped and its contents are run as commands on the same stack,
 * so macros can nest; a number is left untouched. Nesting is
 * depth-limited so a self-re-executing macro (e.g. [[dx]x]dx) errors out
 * instead of overflowing the C stack.
 * @param s Stack to modify (non-empty)
 * @param regs Register file, forwarded to the macro
 * @param depth Current macro nesting depth
 * @return The stack after the execution
 */
static Stack _calc_exec_top(Stack s, Registers regs, int depth) {
    if (stack_top(s)->kind != VAL_STR)
        return s;
    if (depth >= MAX_MACRO_DEPTH) {
        _calc_error("macro nesting too deep");
        return s;
    }
    Value a;
    s = stack_pop(s, &a);
    s = _calc_parse_depth(s, regs, a.str, depth + 1);
    value_clear(&a);
    return s;
}

/**
 * Performs a comparison conditional (>, <, =, !>, !< or !=), whose register
 * name is given separately (it is the token following the operator). Two
 * values are popped and compared as numbers; when the original top of
 * stack satisfies the comparison against the second value, the named
 * register is executed as a macro (without being popped).
 * @param s Stack to modify
 * @param regs Register file
 * @param op The operator: ">", "<", "=", "!>", "!<" or "!="
 * @param name The register name (one byte)
 * @param depth Current macro nesting depth, forwarded to the executed macro
 * @return The stack after performing the command
 */
static Stack _calc_conditional(Stack s, Registers regs, const char *op, unsigned char name, int depth) {
    if (!stack_hasAtLeast(s, 2)) {
        _calc_error("stack underflow");
        return s;
    }
    if (!_calc_top_two_numeric(s)) {
        _calc_error("non-numeric value");
        return s;
    }
    // compare the original top-of-stack against the second value
    int cmp = mpz_cmp(stack_top(s)->num, stack_second(s)->num);
    Value a, b;
    s = stack_pop(s, &a);
    s = stack_pop(s, &b);
    value_clear(&a);
    value_clear(&b);

    // The operator is ">", "<" or "=", optionally negated by a leading '!'
    // (!>, !< and !=)
    bool neg = op[0] == '!';
    char base = neg ? op[1] : op[0];
    bool fire;
    switch (base) {
        case '>': fire = neg ? cmp <= 0 : cmp > 0; break;
        case '<': fire = neg ? cmp >= 0 : cmp < 0; break;
        default:  fire = neg ? cmp != 0 : cmp == 0; break;  // '='
    }
    if (!fire)
        return s;

    // Fire: execute the register like "l r x" — push a copy of its top
    // (0 when the register is empty), then run it if it is a string. The
    // register itself is not popped.
    Stack rs = register_stack(regs, name);
    if (stack_hasAtLeast(rs, 1))
        s = stack_push_value(s, *stack_top(rs));
    else
        s = _calc_push_zero(s);
    return _calc_exec_top(s, regs, depth);
}

/**
 * Performs a single command
 * @param s Stack to modify
 * @param regs Register file to modify (forwarded to x for macros)
 * @param command A single part of the input (e.g. an operand, an operator or
 * an instruction)
 * @param depth Current macro nesting depth, forwarded to x
 * @return The top of the stack after performing the command
 */
static Stack _calc_proceed(Stack s, Registers regs, const char* command, int depth) {
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

        s = stack_push_num(s, number);
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
        if (!_calc_top_two_numeric(s)) {
            _calc_error("non-numeric value");
            return s;
        }
        if (mpz_cmp_si(stack_top(s)->num, 0) == 0) {
            _calc_error("divide by zero");
            return s;
        }
        Value a, b;
        mpz_t quotient, remainder;
        s = stack_pop(s, &a);  // Pop divisor (top of stack)
        s = stack_pop(s, &b);  // Pop dividend (next on stack)

        mpz_init(quotient);
        mpz_init(remainder);
        mpz_tdiv_qr(quotient, remainder, b.num, a.num);

        // Push results: quotient first, then remainder
        s = stack_push_num(s, quotient);
        s = stack_push_num(s, remainder);

        value_clear(&a);
        value_clear(&b);
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
        if (!_calc_top_two_numeric(s)) {
            _calc_error("non-numeric value");
            return s;
        }
        // A positive exponent that does not fit in an unsigned long cannot
        // be computed; reject it before popping so the operands stay on the
        // stack (dc reports "exponent too large in raise")
        if (mpz_sgn(stack_top(s)->num) > 0 && !mpz_fits_ulong_p(stack_top(s)->num)) {
            _calc_error("exponent too large in raise");
            return s;
        }
        // Cap the result size: base^exp has about exp * bits(base) bits, and
        // beyond MAX_POW_BITS GMP would attempt a huge allocation and abort
        // on failure. Bases 0 and +-1 are exact for any exponent and stay
        // instant. Checked before popping so the operands stay on the stack.
        if (mpz_sgn(stack_top(s)->num) > 0 && mpz_cmpabs_ui(stack_second(s)->num, 1) > 0) {
            size_t result_bits;
            if (ckd_mul(&result_bits, (size_t) mpz_get_ui(stack_top(s)->num),
                        mpz_sizeinbase(stack_second(s)->num, 2))
                || result_bits > MAX_POW_BITS) {
                _calc_error("exponent too large in raise");
                return s;
            }
        }
        Value a;
        s = stack_pop(s, &a);
        if (mpz_sgn(a.num) < 0) {
            // Negative exponent
            // at the default scale 0 that is the truncation toward zero of
            // the rational result, i.e. 0 unless the base is 1 or _1
            if (mpz_cmp_si(stack_top(s)->num, 1) == 0)
                mpz_set_ui(stack_top(s)->num, 1);
            else if (mpz_cmp_si(stack_top(s)->num, -1) == 0)
                mpz_set_si(stack_top(s)->num, mpz_odd_p(a.num) ? -1 : 1);
            else
                mpz_set_ui(stack_top(s)->num, 0);
        } else {
            mpz_pow_ui(stack_top(s)->num, stack_top(s)->num, mpz_get_ui(a.num));
        }
        value_clear(&a);
        return s;
    }

    // Square root
    if (strcmp(command, "v") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        if (stack_top(s)->kind != VAL_NUM) {
            _calc_error("non-numeric value");
            return s;
        }
        if (mpz_sgn(stack_top(s)->num) < 0) {
            _calc_error("square root of negative number");
            return s;
        }
        mpz_root(stack_top(s)->num, stack_top(s)->num, 2);
        return s;
    }

    // Clear the stack
    if (strcmp(command, "c") == 0)
        return stack_clear(s);

    // Print the top value (a string is printed as-is)
    if (strcmp(command, "p") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        _calc_print(stack_top(s), true);
        return s;
    }

    // Pop the top value and print it without a trailing newline
    if (strcmp(command, "n") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        Value a;
        s = stack_pop(s, &a);
        _calc_print(&a, false);
        value_clear(&a);
        return s;
    }

    // Pop the top value and print it without a trailing newline: a string
    // is printed as-is, a number as a string of bytes, most significant
    // byte of its absolute value first
    if (strcmp(command, "P") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        Value a;
        s = stack_pop(s, &a);
        if (a.kind == VAL_STR)
            fputs(a.str, stdout);
        else
            _calc_print_bytes(a.num);
        value_clear(&a);
        return s;
    }

    // Print all the stack
    if (strcmp(command, "f") == 0) {
        for (size_t i = s->size; i-- > 0; )
            _calc_print(&s->vals[i], true);
        return s;
    }

    // Duplicate the value at the top
    if (strcmp(command, "d") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        return stack_push_value(s, *stack_top(s));
    }

    // Swap the top two values of the stack (a no-op with fewer than two
    // values, as in dc)
    if (strcmp(command, "r") == 0) {
        if (!stack_hasAtLeast(s, 2))
            return s;
        Value tmp = *stack_top(s);
        *stack_top(s) = *stack_second(s);
        *stack_second(s) = tmp;
        return s;
    }

    // Execute the top value as a macro (x): a string is popped and its
    // contents are run as commands on the same stack, so macros can nest.
    // A number is left untouched.
    if (strcmp(command, "x") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        return _calc_exec_top(s, regs, depth);
    }

    // q: quit two levels of macro execution (the current macro and its
    // invoker); issued at the top level or in a macro called directly from
    // the top level, it exits the program. A numeric top of stack
    // is not popped.
    if (strcmp(command, "q") == 0) {
        _calc_quit = 2;
        if (depth < 2)
            _calc_exitAll = true;
        return s;
    }

    // Q: pop a count N and quit N levels of macro execution; it never exits
    // the program (an error when N exceeds the current depth)
    if (strcmp(command, "Q") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        if (stack_top(s)->kind != VAL_NUM || mpz_sgn(stack_top(s)->num) < 0
            || !mpz_fits_slong_p(stack_top(s)->num)) {
            _calc_error("Q command requires a number >= 0");
            return s;
        }
        Value n;
        s = stack_pop(s, &n);
        if (mpz_get_si(n.num) > depth)
            _calc_error("Q command argument exceeded string execution depth");
        else
            _calc_quit = (int) mpz_get_si(n.num);
        value_clear(&n);
        return s;
    }

    _calc_errorf("'%s' unimplemented", command);
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

    // the two-character conditionals !>, !< and !=
    if (c1 == '!' && (c2 == '>' || c2 == '<' || c2 == '='))
        return false;

    return true;
}

/**
 * Find the first command on the string
 * @param token An allocated string to store the token (a string literal is
 * stored without its outer brackets)
 * @param str String to search the element on (operator, operand or command)
 * @param is_string Set to true when the token is a [...] string literal
 * @return Pointer to str after the extracted token, or NULL at end of input
 * or on an unterminated string literal
 */
static char *_calc_firstToken(char *token, char *str, bool *is_string) {
    size_t i = 0;

    *is_string = false;

    if (!token)
        return NULL;

    // End of input: clear the token so the caller's loop stops
    if (!str) {
        token[0] = '\0';
        return NULL;
    }

    // Ignore leading spaces, tabs and new lines
    while (*str == ' ' || *str == '\t' || *str == '\n')
        str++;

    if (*str == '\0') {
        token[0] = '\0';
        return NULL;
    }

    // String literal: copy everything up to the matching ']', keeping inner
    // bracket pairs (nesting is allowed: [[a]b] is the string "[a]b")
    if (*str == '[') {
        int depth = 1;
        char *p = str + 1;

        token[0] = '\0';
        while (*p != '\0') {
            if (*p == ']') {
                if (--depth == 0) {
                    p++;
                    break;
                }
            } else if (*p == '[') {
                depth++;
            }
            token[i++] = *p++;
            token[i] = '\0';
        }
        if (depth != 0) {
            _calc_error("unterminated string");
            token[0] = '\0';
            return NULL;
        }
        *is_string = true;
        return p;
    }

    token[i] = '\0';

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
 * Fetch the register name following a register command letter (the next
 * token, with or without a separating space). A comment is never a name:
 * a '#' token means the rest of the line is a comment, so the command is
 * left without a name.
 * @param ptr Input position, advanced past the name token
 * @param token Token buffer, overwritten with the name token
 * @return The register name, or -1 when there is none (error reported)
 */
static int _calc_register_name(char **ptr, char *token) {
    bool is_string;
    *ptr = _calc_firstToken(token, *ptr, &is_string);
    if (*ptr == NULL || is_string || token[0] == '#' || token[1] != '\0') {
        _calc_error("expected a single-character register name");
        return -1;
    }
    return (unsigned char) token[0];
}

/**
 * Performs a set of commands (e.g. "2 3*p") at a given macro nesting depth
 * @param s Stack to modify
 * @param regs Register file to modify
 * @param string Commands to execute
 * @param depth Current macro nesting depth (0 for top-level input)
 * @return The top of the stack after performing the commands
 */
static Stack _calc_parse_depth(Stack s, Registers regs, char* string, int depth) {
    char *ptr = string;
    size_t tokcap;
    if (ckd_add(&tokcap, strlen(string), (size_t) 1))
        rpn_fatal("input too large");

    char *token = calloc(tokcap, sizeof(char));
    if (!token)
        rpn_fatal("memory allocation error");

    bool is_string;
    for (;;) {
        // q/Q asked to unwind this level: abandon the rest of the input
        if (_calc_quit > 0) {
            _calc_quit--;
            break;
        }

        ptr = _calc_firstToken(token, ptr, &is_string);
        if (ptr == NULL)
            break;

        // A [...] string literal is pushed as-is, even when empty or
        // starting with '#'
        if (is_string) {
            s = stack_push_str(s, token);
            continue;
        }

        // Comment
        if (token[0] == '#')
            break;

        // Register commands (s, l, S, L, : and ;) take the register name as
        // the next token, with or without a separating space (sa and s a
        // both name register a). Such a token is always a single char: the
        // tokenizer splits after any non-digit.
        if (strchr("slSL:;", token[0]) != NULL) {
            char cmd = token[0];
            int name = _calc_register_name(&ptr, token);
            if (name < 0)
                break;
            s = _calc_register_cmd(s, regs, cmd, (unsigned char) name);
            continue;
        }

        // Comparison conditionals (>, <, =, !>, !<, !=) also take the register
        // name as the next token. A token starting with >, < or = is always a
        // single char (the tokenizer splits after any non-digit); a token
        // starting with ! is two chars only for the !>, !< and != operators.
        if (strchr("><=", token[0]) != NULL
            || (token[0] == '!' && token[1] != '\0' && token[2] == '\0'
                && strchr("><=", token[1]) != NULL)) {
            char op[3] = { token[0], token[1], '\0' };
            int name = _calc_register_name(&ptr, token);
            if (name < 0)
                break;
            s = _calc_conditional(s, regs, op, (unsigned char) name, depth);
            continue;
        }

        s = _calc_proceed(s, regs, token, depth);
    }

    free(token);
    return s;
}

/**
 * Performs a set of commands (e.g. "2 3*p")
 * @param s Stack to modify
 * @param regs Register file to modify
 * @param string Commands to execute
 * @return The top of the stack after performing the commands
 */
Stack calc_parse(Stack s, Registers regs, char* string) {
    return _calc_parse_depth(s, regs, string, 0);
}
