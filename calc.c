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
#include <stdckdint.h>

#include <gmp.h>

#include "calc.h"
#include "io.h"
#include "number.h"
#include "stack.h"

/* Maximum nesting depth of macro execution (x). A deeper nesting is
 * rejected with an error instead of overflowing the C stack. */
#define MAX_MACRO_DEPTH 1000

static bool _calc_errorFlag = false;

/* The scale factor set by k: number of fractional digits produced by /, %,
 * ~, ^ with a negative exponent and v */
static long _calc_scale = 0;

/* The input base (i) and output base (o), both in 2..16; numbers are stored
 * base 10 internally and converted at parse/print time */
static long _calc_in_base = 10;
static long _calc_out_base = 10;

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

/**
 * Whether the top two values of the stack are both numbers
 * @param s Stack to inspect
 * @return true when both the top and the second values are numbers
 */
static bool _calc_top_two_numeric(Stack s) {
    return stack_top(s)->kind == VAL_NUM && stack_second(s)->kind == VAL_NUM;
}

typedef const char *(*NumberBinop)(const Value *, const Value *, long, Value *);

/**
 * Apply a scale-aware binary operation to the top two values of the stack;
 * on error the operands stay on the stack
 * @param s Stack to modify
 * @param op The operation (e.g. number_add)
 * @return The stack after the operation
 */
static Stack _calc_num_binop(Stack s, NumberBinop op) {
    if (!stack_hasAtLeast(s, 2)) {
        _calc_error("stack underflow");
        return s;
    }
    if (!_calc_top_two_numeric(s)) {
        _calc_error("non-numeric value");
        return s;
    }
    Value out;
    const char *err = op(stack_second(s), stack_top(s), _calc_scale, &out);
    if (err) {
        _calc_error(err);
        return s;
    }
    Value a, b;
    s = stack_pop(s, &b);
    s = stack_pop(s, &a);
    value_clear(&a);
    value_clear(&b);
    s = stack_push_value(s, out);
    value_clear(&out);
    return s;
}

/**
 * Print a value in the output base: a string as-is, a number with its scale
 * @param v Value to print
 * @param newline Whether to append a trailing newline
 */
static void _calc_print(const Value *v, bool newline) {
    if (v->kind == VAL_STR) {
        fputs(v->str, stdout);
        if (newline)
            putchar('\n');
    } else {
        number_print(v, newline, _calc_out_base);
    }
}

static Stack _calc_parse_depth(Stack s, Registers regs, char *string, int depth);

/**
 * Set idx to the integer part of a number value. idx must already be
 * initialized by the caller (which also clears it on every path)
 * @param v Value to convert
 * @param idx Output mpz, already initialized
 * @return true when v is a number; false otherwise
 */
static bool _calc_int_part_into(const Value *v, mpz_t idx) {
    if (v->kind != VAL_NUM)
        return false;
    number_int_part(v, idx);
    return true;
}

/**
 * Whether a value is usable as an array index — a nonnegative number (its
 * integer part is the index, so 1.5 indexes cell 1, but _0.5 is rejected
 * because its value is negative) — and set idx to that integer part.
 * idx must already be initialized by the caller
 * @param v Value to inspect
 * @param idx Output mpz, already initialized, set on success
 * @return true when v is a nonnegative number; false otherwise
 */
static bool _calc_index_mpz(const Value *v, mpz_t idx) {
    if (!_calc_int_part_into(v, idx))
        return false;
    return mpz_sgn(v->num) >= 0;
}

/**
 * Pop the top value as a base (the i and o commands): its integer part must
 * be a number in 2..16. On error the value stays on the stack.
 * @param s Stack to modify
 * @param slot The base variable to set on success
 * @param low_err Error message when the value is not a number or below 2
 * @param high_err Error message when the value exceeds 16
 * @return The stack after the command
 */
static Stack _calc_pop_base(Stack s, long *slot, const char *low_err, const char *high_err) {
    if (!stack_hasAtLeast(s, 1)) {
        _calc_error("stack underflow");
        return s;
    }
    mpz_t ip;
    mpz_init(ip);
    const char *err = NULL;
    if (!_calc_int_part_into(stack_top(s), ip))
        err = low_err;
    else if (!mpz_fits_slong_p(ip) || mpz_cmp_ui(ip, 2) < 0)
        err = low_err;
    else if (mpz_cmp_ui(ip, 16) > 0)
        err = high_err;
    if (err) {
        _calc_error(err);
        mpz_clear(ip);
        return s;
    }
    *slot = mpz_get_si(ip);
    mpz_clear(ip);
    Value a;
    s = stack_pop(s, &a);
    value_clear(&a);
    return s;
}

/**
 * Push a long as a number value onto the stack at scale 0
 * @param s Stack to modify
 * @param v The value to push
 * @return The stack with v on top
 */
static Stack _calc_push_long(Stack s, long v) {
    mpz_t m;
    mpz_init_set_si(m, v);
    s = stack_push_num(s, m);
    mpz_clear(m);
    return s;
}

/**
 * Pop the top value into *out, reporting an underflow error and leaving the
 * stack unchanged when the stack is empty
 * @param s Stack to pop from (updated in place)
 * @param out The popped value (ownership moves to the caller on success)
 * @return true on success, false on underflow (stack unchanged)
 */
static bool _calc_pop_one(Stack *s, Value *out) {
    if (!stack_hasAtLeast(*s, 1)) {
        _calc_error("stack underflow");
        return false;
    }
    *s = stack_pop(*s, out);
    return true;
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
            return _calc_push_long(s, 0);
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
            mpz_t idx;
            mpz_init(idx);
            if (!_calc_index_mpz(stack_top(s), idx)) {
                _calc_error("array index must be a nonnegative integer");
                mpz_clear(idx);
                return s;
            }
            Value index, val;
            s = stack_pop(s, &index);  // Pop the index (top of stack)
            s = stack_pop(s, &val);    // Pop the value
            register_array_store(regs, name, idx, val);  // moves val
            mpz_clear(idx);
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
            mpz_t idx;
            mpz_init(idx);
            if (!_calc_index_mpz(stack_top(s), idx)) {
                _calc_error("array index must be a nonnegative integer");
                mpz_clear(idx);
                return s;
            }
            Value index;
            s = stack_pop(s, &index);
            const Value *cell = register_array_find(regs, name, idx);
            mpz_clear(idx);
            if (cell)
                s = stack_push_value(s, *cell);
            else
                s = _calc_push_long(s, 0);
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
    int cmp = number_cmp(stack_top(s), stack_second(s));
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
        s = _calc_push_long(s, 0);
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
    // A number (integer or decimal: 12, _3, 1.5, .5, 1.; digit chars 0-9A-F
    // are read in the current input base)
    if (number_digit_value(command[0]) >= 0 || command[0] == '_'
        || command[0] == '.') {
        Value v;
        if (!number_parse(command, _calc_in_base, &v)) {
            _calc_error("scale too large");
            return s;
        }
        s = stack_push_value(s, v);
        value_clear(&v);
        return s;
    }

    // Arithmetic operators
    if (strcmp(command, "+") == 0)
        return _calc_num_binop(s, number_add);
    if (strcmp(command, "-") == 0)
        return _calc_num_binop(s, number_sub);
    if (strcmp(command, "*") == 0)
        return _calc_num_binop(s, number_mul);
    if (strcmp(command, "/") == 0)
        return _calc_num_binop(s, number_div);
    if (strcmp(command, "%") == 0)
        return _calc_num_binop(s, number_mod);

    // Division with remainder (~): quotient at scale k pushed first, then
    // the remainder (scale max(sa, sb+k)) on top
    if (strcmp(command, "~") == 0) {
        if (!stack_hasAtLeast(s, 2)) {
            _calc_error("stack underflow");
            return s;
        }
        if (!_calc_top_two_numeric(s)) {
            _calc_error("non-numeric value");
            return s;
        }
        Value q, r;
        const char *err = number_divmod(stack_second(s), stack_top(s), _calc_scale, &q, &r);
        if (err) {
            _calc_error(err);
            return s;
        }
        Value a, b;
        s = stack_pop(s, &b);  // Pop divisor (top of stack)
        s = stack_pop(s, &a);  // Pop dividend (next on stack)
        value_clear(&a);
        value_clear(&b);
        s = stack_push_value(s, q);
        s = stack_push_value(s, r);
        value_clear(&q);
        value_clear(&r);
        return s;
    }

    // Exponentiation
    if (strcmp(command, "^") == 0)
        return _calc_num_binop(s, number_pow);

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
        Value out;
        const char *err = number_sqrt(stack_top(s), _calc_scale, &out);
        if (err) {
            _calc_error(err);
            return s;
        }
        Value a;
        s = stack_pop(s, &a);
        value_clear(&a);
        s = stack_push_value(s, out);
        value_clear(&out);
        return s;
    }

    // k: pop the scale factor (the integer part of the top value); it must
    // be a nonnegative number not exceeding MAX_SCALE
    if (strcmp(command, "k") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        long ns;
        const char *err = number_scale_of(stack_top(s), &ns);
        if (err) {
            _calc_error(err);
            return s;
        }
        _calc_scale = ns;
        Value a;
        s = stack_pop(s, &a);
        value_clear(&a);
        return s;
    }

    // K: push the current scale factor
    if (strcmp(command, "K") == 0)
        return _calc_push_long(s, _calc_scale);

    // i: pop the input base (the integer part of the top value, 2..16)
    if (strcmp(command, "i") == 0)
        return _calc_pop_base(s, &_calc_in_base,
                              "input base must be a number between 2 and 16 (inclusive)",
                              "input base must be a number between 2 and 16 (inclusive)");

    // o: pop the output base (the integer part of the top value, 2..16)
    if (strcmp(command, "o") == 0)
        return _calc_pop_base(s, &_calc_out_base,
                              "output base must be a number greater than 1",
                              "output base must be a number between 2 and 16 (inclusive)");

    // I: push the current input base
    if (strcmp(command, "I") == 0)
        return _calc_push_long(s, _calc_in_base);

    // O: push the current output base
    if (strcmp(command, "O") == 0)
        return _calc_push_long(s, _calc_out_base);

    // z: push the depth of the stack (without popping)
    if (strcmp(command, "z") == 0)
        return _calc_push_long(s, (long) s->size);

    // Z: pop the top value and push its length: the number of decimal digits
    // of a number (the sign and the point are not counted), or the number of
    // characters of a string
    if (strcmp(command, "Z") == 0) {
        Value a;
        if (!_calc_pop_one(&s, &a))
            return s;
        long len = number_length(&a);
        value_clear(&a);
        return _calc_push_long(s, len);
    }

    // X: pop the top value and push its scale (the number of fractional
    // digits; 0 for a string)
    if (strcmp(command, "X") == 0) {
        Value a;
        if (!_calc_pop_one(&s, &a))
            return s;
        long scale = a.kind == VAL_NUM ? a.scale : 0;
        value_clear(&a);
        return _calc_push_long(s, scale);
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
        Value a;
        if (!_calc_pop_one(&s, &a))
            return s;
        _calc_print(&a, false);
        value_clear(&a);
        return s;
    }

    // Pop the top value and print it without a trailing newline: a string
    // is printed as-is, a number as a string of bytes, most significant
    // byte of its absolute value first
    if (strcmp(command, "P") == 0) {
        Value a;
        if (!_calc_pop_one(&s, &a))
            return s;
        if (a.kind == VAL_STR)
            fputs(a.str, stdout);
        else
            number_print_bytes(&a);
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
    // the program. An over-depth count is an error (popped) but still quits
    // the available levels, min(N, depth). The count is the integer part of
    // the top value, which must be >= 0.
    if (strcmp(command, "Q") == 0) {
        if (!stack_hasAtLeast(s, 1)) {
            _calc_error("stack underflow");
            return s;
        }
        mpz_t ip;
        mpz_init(ip);
        if (!_calc_int_part_into(stack_top(s), ip) || mpz_sgn(ip) < 0 || !mpz_fits_slong_p(ip)) {
            _calc_error("Q command requires a number >= 0");
            mpz_clear(ip);
            return s;
        }
        Value n;
        s = stack_pop(s, &n);
        long count = mpz_get_si(ip);
        mpz_clear(ip);
        if (count > depth)
            _calc_error("Q command argument exceeded string execution depth");
        _calc_quit = (int) (count < depth ? count : depth);
        value_clear(&n);
        return s;
    }

    _calc_errorf("'%s' unimplemented", command);
    return s;
}

/**
 * Whether c continues the current token
 * @param token Token built so far
 * @param len Length of token
 * @param c The next character
 * @return true when c belongs to the token being built
 */
static bool _calc_token_continues(const char *token, size_t len, char c) {
    // A digit char (0-9 or hex letter A-F) continues a number (possibly
    // negative: _ prefix, or after a .)
    if (number_digit_value(c) >= 0)
        return len > 0 && (token[len-1] == '_' || token[len-1] == '.'
                           || number_digit_value(token[len-1]) >= 0);

    // A '.' continues a number token that does not contain one yet: 1.5, .5
    // and 1. are single tokens, and a second '.' starts a new token
    // (1..2 is read as 1. then .2)
    if (c == '.') {
        if (strchr(token, '.') != NULL)
            return false;
        for (size_t i = 0; i < len; i++)
            if (token[i] != '_' && number_digit_value(token[i]) < 0)
                return false;
        return true;
    }

    // The two-character conditionals !>, !< and !=
    if ((c == '>' || c == '<' || c == '=') && len == 1 && token[0] == '!')
        return true;

    return false;
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
        // Stop when the next char does not belong to the current token
        if (i > 0 && !_calc_token_continues(token, i, *str))
            break;
        token[i++] = *str++;
        token[i] = '\0';
    }

    return str;
}

/**
 * Fetch the register name following a register command letter or a
 * conditional operator: the single character after any whitespace, as in dc
 * (`sA2` names register A and leaves the 2 to be parsed as a number). A
 * comment or a string literal is never a name: a '#' char means the rest of
 * the line is a comment, so the command is left without a name.
 * @param ptr Input position, advanced past the name character
 * @return The register name, or -1 when there is none (error reported)
 */
static int _calc_register_name(char **ptr) {
    while (**ptr == ' ' || **ptr == '\t' || **ptr == '\n')
        (*ptr)++;
    char c = **ptr;
    if (c == '\0' || c == '#' || c == '[') {
        _calc_error("expected a single-character register name");
        return -1;
    }
    (*ptr)++;
    return (unsigned char) c;
}

/**
 * Whether a token is a comparison conditional operator (>, <, =, !>, !<, !=)
 * @param token Token to inspect
 * @return Pointer to the operator within token, or NULL when not a conditional
 */
static const char *_calc_conditional_op(const char *token) {
    // >, < and = are always single-char tokens (the tokenizer splits after
    // any non-digit)
    if (strchr("><=", token[0]) != NULL)
        return token;
    // !>, !< and != are the only two-char tokens starting with !
    if (token[0] == '!' && token[1] != '\0' && token[2] == '\0'
        && strchr("><=", token[1]) != NULL)
        return token;
    return NULL;
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
        // the next character, with or without a separating space (sa and s a
        // both name register a; sA2 names register A and pushes 2, as in dc)
        if (strchr("slSL:;", token[0]) != NULL) {
            char cmd = token[0];
            int name = _calc_register_name(&ptr);
            if (name < 0)
                break;
            s = _calc_register_cmd(s, regs, cmd, (unsigned char) name);
            continue;
        }

        // Comparison conditionals (>, <, =, !>, !<, !=) also take the register
        // name as the next character
        const char *op = _calc_conditional_op(token);
        if (op != NULL) {
            char opbuf[3] = { op[0], op[1], '\0' };
            int name = _calc_register_name(&ptr);
            if (name < 0)
                break;
            s = _calc_conditional(s, regs, opbuf, (unsigned char) name, depth);
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

/**
 * Performs the commands of a -e expression. Like dc, -e strings are
 * executed one macro level above the top level (depth 1), so q/Q counting
 * in -e mode matches dc
 * @param s Stack to modify
 * @param regs Register file to modify
 * @param string Commands to execute
 * @return The top of the stack after performing the commands
 */
Stack calc_parse_expression(Stack s, Registers regs, char* string) {
    return _calc_parse_depth(s, regs, string, 1);
}
