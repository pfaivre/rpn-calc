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

#include <string.h>

#include "print.h"
#include "calc.h"    // calc_out_base, calc_report_errorf
#include "io.h"      // emit_char, emit_raw
#include "number.h"  // number_print, number_print_bytes

/**
 * Check that the stack holds at least n values, reporting an underflow
 * error once when it does not
 * @param s Stack to check
 * @param n Minimum number of values required
 * @return true when the stack holds at least n values
 */
static bool _print_need(Stack s, int n) {
    if (!stack_has_at_least(s, n)) {
        calc_report_errorf("stack underflow");
        return false;
    }
    return true;
}

/**
 * Pop the top value into *out, reporting an underflow error and leaving the
 * stack unchanged when the stack is empty
 * @param s Stack to pop from (updated in place)
 * @param out The popped value (ownership moves to the caller on success)
 * @return true on success, false on underflow (stack unchanged)
 */
static bool _print_pop_one(Stack *s, Value *out) {
    if (!_print_need(*s, 1))
        return false;
    *s = stack_pop(*s, out);
    return true;
}

/**
 * Print a value in the output base: a string as-is, a number with its scale
 * @param v Value to print
 * @param newline Whether to append a trailing newline
 */
static void _print_value(const Value *v, bool newline) {
    if (v->kind == VAL_STR) {
        emit_raw(v->str, strlen(v->str));  // strings are never wrapped
        if (newline)
            emit_char('\n');
    } else {
        number_print(v, newline, calc_out_base());
    }
}

Stack print_cmd_top(Stack s, Registers regs, int depth) {
    if (!_print_need(s, 1))
        return s;
    _print_value(stack_top(s), true);
    return s;
}

Stack print_cmd_nonl(Stack s, Registers regs, int depth) {
    Value a;
    if (!_print_pop_one(&s, &a))
        return s;
    _print_value(&a, false);
    value_clear(&a);
    return s;
}

Stack print_cmd_bytes(Stack s, Registers regs, int depth) {
    Value a;
    if (!_print_pop_one(&s, &a))
        return s;
    if (a.kind == VAL_STR)
        emit_raw(a.str, strlen(a.str));  // strings are never wrapped
    else
        number_print_bytes(&a);
    value_clear(&a);
    return s;
}

Stack print_cmd_all(Stack s, Registers regs, int depth) {
    for (size_t i = s->size; i-- > 0; )
        _print_value(&s->vals[i], true);
    return s;
}
