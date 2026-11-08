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
#include <stdckdint.h>

#include <gmp.h>

#include "io.h"
#include "stack.h"
#include "number.h"

/* Maximum size of a ^ result, in bits (32 MiB). Beyond this, computing the
 * full power risks exhausting memory, so the exponent is rejected up front
 * with a graceful error instead. */
#define MAX_POW_BITS ((size_t) 1 << 28)

/* Largest exponent e for which 10^e stays within MAX_POW_BITS bits
 * (e * log2(10) <= MAX_POW_BITS). 334/100 > log2(10), so this is conservative. */
#define MAX_POW10_EXP ((long) ((MAX_POW_BITS * 100) / 334))

/** Set out to the number (num, scale), copying num */
static void number_set(Value *out, const mpz_t num, long scale) {
    out->kind = VAL_NUM;
    mpz_init_set(out->num, num);
    out->scale = scale;
}

/** r = 10^e (e >= 0; callers keep e bounded via the MAX_POW_BITS / MAX_SCALE
 * checks, so the result fits in memory) */
static void pow10(mpz_t r, long e) {
    mpz_ui_pow_ui(r, 10, (unsigned long) e);
}

/** r = v->num * 10^(s - v->scale), for s >= v->scale */
static void num_at_scale(mpz_t r, const Value *v, long s) {
    if (s == v->scale) {
        mpz_set(r, v->num);
    } else {
        mpz_t p;
        mpz_init(p);
        pow10(p, s - v->scale);
        mpz_mul(r, v->num, p);
        mpz_clear(p);
    }
}

bool number_parse(const char *token, Value *out) {
    bool neg = token[0] == '_';
    const char *p = token + (neg ? 1 : 0);
    const char *dot = strchr(p, '.');
    long scale = dot ? (long) strlen(dot + 1) : 0;
    if (scale > MAX_SCALE)
        return false;

    // The digits of the token without the point
    size_t len = strlen(p);
    size_t dlen, bufsize;
    if (ckd_sub(&dlen, len, dot ? (size_t) 1 : (size_t) 0)
        || ckd_add(&bufsize, dlen, (size_t) 1))
        rpn_fatal("input too large");
    char *digits = malloc(bufsize);
    if (!digits)
        rpn_fatal("memory allocation error");
    size_t j = 0;
    for (size_t i = 0; i < len; i++)
        if (p[i] != '.')
            digits[j++] = p[i];
    digits[j] = '\0';

    mpz_t num;
    mpz_init(num);
    if (j == 0)
        mpz_set_ui(num, 0);
    else
        mpz_set_str(num, digits, 10);  // all digits: cannot fail
    if (neg)
        mpz_neg(num, num);
    free(digits);

    number_set(out, num, scale);
    mpz_clear(num);
    return true;
}

void number_print(const Value *v, bool newline) {
    if (mpz_sgn(v->num) == 0) {
        fputs("0", stdout);  // zero prints as "0", whatever its scale or sign
    } else {
        size_t bufsize = mpz_sizeinbase(v->num, 10) + 2;  // sign + NUL
        char *digits = malloc(bufsize);
        if (!digits)
            rpn_fatal("memory allocation error");
        mpz_get_str(digits, 10, v->num);
        size_t len = strlen(digits);
        const char *d = digits;
        if (d[0] == '-') {
            putchar('-');
            d++;
            len--;
        }
        long scale = v->scale;
        if (scale == 0) {
            fputs(d, stdout);
        } else if ((size_t) scale < len) {
            fwrite(d, 1, len - (size_t) scale, stdout);
            putchar('.');
            fputs(d + (len - (size_t) scale), stdout);
        } else {
            // fewer digits than the scale: no integer part, no leading zero
            putchar('.');
            for (long zeros = scale - (long) len; zeros > 0; zeros--)
                putchar('0');
            fputs(d, stdout);
        }
        free(digits);
    }
    if (newline)
        putchar('\n');
}

void number_print_bytes(const Value *v) {
    mpz_t ip;
    mpz_init(ip);
    number_int_part(v, ip);
    mpz_abs(ip, ip);
    size_t nbytes = mpz_sizeinbase(ip, 256);  // at least 1, even for zero
    unsigned char *bytes = malloc(nbytes);
    if (!bytes)
        rpn_fatal("memory allocation error");
    size_t count = 0;
    mpz_export(bytes, &count, 1, 1, 1, 0, ip);
    if (count == 0)
        putchar(0);  // zero prints as a single NUL byte
    else
        fwrite(bytes, 1, count, stdout);
    free(bytes);
    mpz_clear(ip);
}

int number_cmp(const Value *a, const Value *b) {
    long s = a->scale > b->scale ? a->scale : b->scale;
    mpz_t an, bn;
    mpz_init(an);
    mpz_init(bn);
    num_at_scale(an, a, s);
    num_at_scale(bn, b, s);
    int c = mpz_cmp(an, bn);
    mpz_clear(an);
    mpz_clear(bn);
    return c;
}

void number_int_part(const Value *v, mpz_t out) {
    if (v->scale == 0) {
        mpz_set(out, v->num);
    } else {
        mpz_t p;
        mpz_init(p);
        pow10(p, v->scale);
        mpz_tdiv_q(out, v->num, p);
        mpz_clear(p);
    }
}

/** a +/- b with result scale max(sa, sb) */
static const char *number_addsub(const Value *a, const Value *b, bool sub, Value *out) {
    long rs = a->scale > b->scale ? a->scale : b->scale;
    mpz_t an, bn, rn;
    mpz_init(an);
    mpz_init(bn);
    mpz_init(rn);
    num_at_scale(an, a, rs);
    num_at_scale(bn, b, rs);
    if (sub)
        mpz_sub(rn, an, bn);
    else
        mpz_add(rn, an, bn);
    number_set(out, rn, rs);
    mpz_clear(an);
    mpz_clear(bn);
    mpz_clear(rn);
    return NULL;
}

const char *number_add(const Value *a, const Value *b, long k, Value *out) {
    (void) k;  // the scale factor does not apply to + and -
    return number_addsub(a, b, false, out);
}

const char *number_sub(const Value *a, const Value *b, long k, Value *out) {
    (void) k;
    return number_addsub(a, b, true, out);
}

const char *number_mul(const Value *a, const Value *b, long k, Value *out) {
    // The result scale is min(sa+sb, max(sa, sb, k))
    long sum = a->scale + b->scale;  // at most 2*MAX_SCALE: no overflow
    long cap = a->scale;
    if (b->scale > cap)
        cap = b->scale;
    if (k > cap)
        cap = k;
    long rs = sum < cap ? sum : cap;

    mpz_t rn, p;
    mpz_init(rn);
    mpz_init(p);
    mpz_mul(rn, a->num, b->num);
    if (sum > rs) {  // truncate the excess fractional digits
        pow10(p, sum - rs);
        mpz_tdiv_q(rn, rn, p);
    }
    number_set(out, rn, rs);
    mpz_clear(rn);
    mpz_clear(p);
    return NULL;
}

/** qn = trunc(a/b) with scale k (b must be nonzero) */
static void div_at_scale(mpz_t qn, const Value *a, const Value *b, long k) {
    long e = k + b->scale - a->scale;
    mpz_t t;
    mpz_init(t);
    if (e >= 0) {
        pow10(t, e);
        mpz_mul(t, a->num, t);
        mpz_tdiv_q(qn, t, b->num);
    } else {
        pow10(t, -e);
        mpz_mul(t, b->num, t);
        mpz_tdiv_q(qn, a->num, t);
    }
    mpz_clear(t);
}

const char *number_div(const Value *a, const Value *b, long k, Value *out) {
    if (mpz_sgn(b->num) == 0)
        return "divide by zero";
    mpz_t qn;
    mpz_init(qn);
    div_at_scale(qn, a, b, k);
    number_set(out, qn, k);
    mpz_clear(qn);
    return NULL;
}

/** rn = a - b*trunc(a/b at scale k), at scale rs (b nonzero, rs >= sb+k) */
static void mod_at_scale(mpz_t rn, const Value *a, const Value *b, long k, long rs) {
    mpz_t qn, an, bq, t;
    mpz_init(qn);
    mpz_init(an);
    mpz_init(bq);
    mpz_init(t);
    div_at_scale(qn, a, b, k);
    num_at_scale(an, a, rs);
    mpz_mul(bq, b->num, qn);
    long sh = rs - b->scale - k;  // >= 0 because rs >= sb + k
    if (sh > 0) {
        pow10(t, sh);
        mpz_mul(bq, bq, t);
    }
    mpz_sub(rn, an, bq);
    mpz_clear(qn);
    mpz_clear(an);
    mpz_clear(bq);
    mpz_clear(t);
}

/** The remainder scale: max(sa, sb + k), capped */
static const char *mod_scale(long sa, long sb, long k, long *rs) {
    long r = sb + k;  // at most 2*MAX_SCALE: no overflow
    if (sa > r)
        r = sa;
    if (r > MAX_SCALE)
        return "scale too large";
    *rs = r;
    return NULL;
}

const char *number_mod(const Value *a, const Value *b, long k, Value *out) {
    if (mpz_sgn(b->num) == 0)
        return "remainder by zero";
    long rs;
    const char *err = mod_scale(a->scale, b->scale, k, &rs);
    if (err)
        return err;
    mpz_t rn;
    mpz_init(rn);
    mod_at_scale(rn, a, b, k, rs);
    number_set(out, rn, rs);
    mpz_clear(rn);
    return NULL;
}

const char *number_divmod(const Value *a, const Value *b, long k, Value *q, Value *r) {
    if (mpz_sgn(b->num) == 0)
        return "divide by zero";
    long rs;
    const char *err = mod_scale(a->scale, b->scale, k, &rs);
    if (err)
        return err;
    mpz_t qn, rn;
    mpz_init(qn);
    mpz_init(rn);
    div_at_scale(qn, a, b, k);
    mod_at_scale(rn, a, b, k, rs);
    number_set(q, qn, k);
    number_set(r, rn, rs);
    mpz_clear(qn);
    mpz_clear(rn);
    return NULL;
}

/** true when base's value is +-1, i.e. |num| == 10^scale */
static bool is_unit_base(const Value *base) {
    mpz_t ten;
    mpz_init(ten);
    pow10(ten, base->scale);
    bool unit = mpz_cmpabs(base->num, ten) == 0;
    mpz_clear(ten);
    return unit;
}

/** rn = the value +-1 of base^e represented at scale s: +-10^s */
static void unit_at_scale(mpz_t rn, const Value *base, const mpz_t e, long s) {
    mpz_set_si(rn, (mpz_sgn(base->num) < 0 && mpz_odd_p(e)) ? -1 : 1);
    mpz_t p;
    mpz_init(p);
    pow10(p, s);
    mpz_mul(rn, rn, p);
    mpz_clear(p);
}

/** Exponent size guard: e (already positive) must fit an unsigned long and
 * n * bits(num) must stay within MAX_POW_BITS, so |num|^n is computable.
 * On success sets *n and returns NULL; otherwise returns the error. */
static const char *pow_exp_guard(const mpz_t e, const mpz_t num, unsigned long *n) {
    if (!mpz_fits_ulong_p(e))
        return "exponent too large in raise";
    *n = mpz_get_ui(e);
    size_t bits;
    if (ckd_mul(&bits, (size_t) *n, mpz_sizeinbase(num, 2)) || bits > MAX_POW_BITS)
        return "exponent too large in raise";
    return NULL;
}

/** rn = trunc(x / 10^shift), shift >= 0, with a zero fast-path when x < 10^shift.
 * bits(x) <= shift*log2(10) implies x < 10^shift; 332/100 < log2(10), safe. */
static void trunc_div_pow10(mpz_t rn, const mpz_t x, long shift) {
    if (shift == 0) {
        mpz_set(rn, x);
        return;
    }
    if (mpz_sizeinbase(x, 2) <= (size_t) shift * 332 / 100) {
        mpz_set_ui(rn, 0);
        return;
    }
    mpz_t p;
    mpz_init(p);
    pow10(p, shift);
    mpz_tdiv_q(rn, x, p);
    mpz_clear(p);
}

const char *number_pow(const Value *base, const Value *exp, long k, Value *out) {
    const char *err = NULL;
    mpz_t e, rn;
    mpz_init(e);
    mpz_init(rn);
    number_int_part(exp, e);  // the exponent is truncated to its integer part
    long sa = base->scale;

    if (mpz_sgn(e) == 0) {
        mpz_set_ui(rn, 1);
        number_set(out, rn, 0);
    } else if (mpz_sgn(e) < 0) {
        // Negative exponent n: result at scale k, value trunc(10^(k+sa*n) / |num|^n),
        // with the sign of the base for odd n. Bases 0 and +-1 are exact for any
        // exponent and stay instant.
        if (is_unit_base(base)) {
            unit_at_scale(rn, base, e, k);
            number_set(out, rn, k);
        } else if (mpz_sgn(base->num) == 0) {
            mpz_set_ui(rn, 0);
            number_set(out, rn, k);
        } else {
            mpz_abs(e, e);
            unsigned long n;
            err = pow_exp_guard(e, base->num, &n);
            if (!err) {
                // e2 = sa*n + k bounds the 10^e2 numerator; cap it so pow10 stays
                // within MAX_POW_BITS bits instead of risking a huge allocation
                long e2 = 0;
                if (ckd_mul(&e2, sa, (long) n) || ckd_add(&e2, e2, k) || e2 > MAX_POW10_EXP)
                    err = "exponent too large in raise";
                else {
                    mpz_t apow, p;
                    mpz_init(apow);
                    mpz_init(p);
                    mpz_abs(apow, base->num);
                    mpz_pow_ui(apow, apow, n);
                    pow10(p, e2);
                    mpz_tdiv_q(rn, p, apow);
                    if (mpz_sgn(base->num) < 0 && (n & 1))
                        mpz_neg(rn, rn);
                    mpz_clear(apow);
                    mpz_clear(p);
                }
            }
            if (!err)
                number_set(out, rn, k);
        }
    } else {
        // Positive exponent n: result scale min(sa*n, max(sa, k)). Bases of value
        // 0 or +-1 are exact for any exponent and stay instant.
        long cap = sa > k ? sa : k;
        if (is_unit_base(base) || mpz_sgn(base->num) == 0) {
            long sa_n = 0;
            bool big = !mpz_fits_ulong_p(e) || ckd_mul(&sa_n, sa, (long) mpz_get_ui(e));
            long rs = big ? cap : (sa_n < cap ? sa_n : cap);  // min(sa*n, cap)
            if (mpz_sgn(base->num) == 0)
                mpz_set_ui(rn, 0);
            else
                unit_at_scale(rn, base, e, rs);
            number_set(out, rn, rs);
        } else {
            long sa_n = 0;
            bool big = !mpz_fits_ulong_p(e) || ckd_mul(&sa_n, sa, (long) mpz_get_ui(e));
            long rs = big ? cap : (sa_n < cap ? sa_n : cap);  // min(sa*n, cap)
            if (big)
                err = "exponent too large in raise";
            else {
                unsigned long n;
                err = pow_exp_guard(e, base->num, &n);
                if (!err) {
                    mpz_t full;
                    mpz_init(full);
                    mpz_pow_ui(full, base->num, n);
                    trunc_div_pow10(rn, full, sa_n - rs);  // shift >= 0
                    mpz_clear(full);
                    number_set(out, rn, rs);
                }
            }
        }
    }

    mpz_clear(e);
    mpz_clear(rn);
    return err;
}

const char *number_sqrt(const Value *a, long k, Value *out) {
    if (mpz_sgn(a->num) < 0)
        return "square root of negative number";
    // Result scale max(sa, k); rn = floor(sqrt(na * 10^(2*rs - sa)))
    long rs = a->scale > k ? a->scale : k;
    long e = 2 * rs - a->scale;  // >= 0, at most 2*MAX_SCALE
    mpz_t t, rn;
    mpz_init(t);
    mpz_init(rn);
    pow10(t, e);
    mpz_mul(t, a->num, t);
    mpz_root(rn, t, 2);
    number_set(out, rn, rs);
    mpz_clear(t);
    mpz_clear(rn);
    return NULL;
}

const char *number_scale_of(const Value *v, long *out) {
    if (v->kind != VAL_NUM)
        return "scale must be a nonnegative number";
    mpz_t ip;
    mpz_init(ip);
    number_int_part(v, ip);
    const char *err = NULL;
    if (mpz_sgn(ip) < 0)
        err = "scale must be a nonnegative number";
    else if (!mpz_fits_slong_p(ip) || mpz_get_si(ip) > MAX_SCALE)
        err = "scale too large";
    else
        *out = mpz_get_si(ip);
    mpz_clear(ip);
    return err;
}
