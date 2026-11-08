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
 * The value of a digit char: 0-9 for '0'-'9', 10-15 for 'A'-'F' (uppercase;
 * lowercase letters are not digit chars)
 * @param c A character
 * @return The digit value, or -1 when c is not a digit char
 */
int number_digit_value(char c);

/**
 * Parse a numeric token (optional leading _, digit chars 0-9A-F, at most one
 * '.') into a number value, reading the digits in the given input base.
 * Numbers are stored base 10 internally (value = num / 10^scale): the
 * integer digits are converted from base, and a fraction of f digits is
 * truncated to f decimal digits. Digit chars keep their face value (A=10
 * ... F=15) even when it reaches the base.
 * @param token The token (guaranteed to match the number shape by the
 * tokenizer)
 * @param base The current input base (2..16)
 * @param out The number value to fill
 * @return false when the scale exceeds MAX_SCALE
 */
bool number_parse(const char *token, long base, Value *out);

/**
 * Print a number in the given output base with its scale: no leading zero
 * before the point (.5, not 0.5), the fraction printed with
 * ceil(log_base(10^scale)) digits, and zero always printed as "0" whatever
 * its scale or sign
 * @param v The number value
 * @param newline Whether to append a trailing newline
 * @param base The current output base (2..16)
 */
void number_print(const Value *v, bool newline, long base);

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
 * The length of a value: the number of decimal digits of a number (the sign
 * and the point are not counted; at least 1, even for 0), or the number of
 * characters of a string
 * @param v The value to measure
 * @return The length of the value
 */
long number_length(const Value *v);

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

/**
 * Modular exponentiation (dc's |): computes base^exp mod mod, with the
 * operands passed as (base, exponent, modulus). The base and exponent are
 * truncated to their integer parts; the modulus is used with its scale.
 * The result scale is max(scale(base), scale(modulus)). Works with
 * arbitrarily large exponents.
 * @param base The base
 * @param exp The exponent
 * @param mod The modulus
 * @param out The result, set on success and untouched on error
 * @return NULL on success or an error message
 */
const char *number_powm(const Value *base, const Value *exp, const Value *mod, Value *out);

#endif
