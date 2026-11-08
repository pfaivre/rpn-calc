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

#ifndef NUMBER_H
#define NUMBER_H

#include <gmp.h>

#include "stack.h"

/**
 * Cap on the scale (number of fractional digits) of any value, set by k or
 * parsed from a literal. Keeps computed results and printed output bounded:
 * a number is num / 10^scale, and 10^scale for a huge scale would be a
 * memory bomb.
 */
#define MAX_SCALE 1000000L

/**
 * Parse a numeric token (optional leading _, digits, at most one '.') into
 * a number value: num holds the digits with the point removed and scale the
 * number of fraction digits, so the value is num / 10^scale
 * @param token The token (guaranteed to match [_]?[0-9]*(\.[0-9]*)? by the
 * tokenizer)
 * @param out The number value to fill
 * @return false when the scale exceeds MAX_SCALE
 */
bool number_parse(const char *token, Value *out);

/**
 * Print a number in decimal with its scale: no leading zero before the
 * point (.5, not 0.5), trailing fractional digits kept (1.50), and zero
 * always printed as "0" whatever its scale or sign
 */
void number_print(const Value *v, bool newline);

/**
 * Print the integer part of a number as a string of bytes, most significant
 * byte of its absolute value first, without a trailing newline; zero prints
 * as a single NUL byte
 */
void number_print_bytes(const Value *v);

/**
 * Compare two numbers by value, ignoring their scales
 * @return <0, 0 or >0 as a is less than, equal to, or greater than b
 */
int number_cmp(const Value *a, const Value *b);

/**
 * The integer part of a number (truncation toward zero)
 * @param v A number value
 * @param out The integer part (initialized by this call)
 */
void number_int_part(const Value *v, mpz_t out);

/**
 * The scale factor from a value (its integer part), for the k command: must be
 * a nonnegative number not exceeding MAX_SCALE
 * @param v The value to read the scale from
 * @param out The scale factor, set on success
 * @return NULL on success, or an error message ("out untouched on error")
 */
const char *number_scale_of(const Value *v, long *out);

/* Arithmetic on numbers. Each writes a fresh number value to out and returns
 * NULL on success or an error message. k is the current scale factor.
 * out is left untouched on error. */

const char *number_add(const Value *a, const Value *b, long k, Value *out);
const char *number_sub(const Value *a, const Value *b, long k, Value *out);
const char *number_mul(const Value *a, const Value *b, long k, Value *out);
const char *number_div(const Value *a, const Value *b, long k, Value *out);
const char *number_mod(const Value *a, const Value *b, long k, Value *out);
const char *number_divmod(const Value *a, const Value *b, long k, Value *q, Value *r);
const char *number_pow(const Value *a, const Value *b, long k, Value *out);
const char *number_sqrt(const Value *a, long k, Value *out);

#endif
