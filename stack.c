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

#include <stdlib.h>
#include <stdckdint.h>

#include <gmp.h>

#include "io.h"
#include "stack.h"

/**
 * Creates a new empty Stack
 */
Stack stack_init() {
    Stack s = malloc(sizeof(StructStack));
    if (!s)
        rpn_fatal("memory allocation error");

    s->vals = NULL;
    s->size = 0;
    s->cap = 0;
    return s;
}

/**
 * Append a value at the top of a stack, growing the backing array as
 * needed (doubling its capacity, with the byte size checked against
 * overflow). Reallocating an array of mpz_t only moves the handles; the
 * limbs they point to are allocated separately and stay valid.
 * @param s Stack to be completed
 * @param val value to add
 * @return the same stack, with the new value on top
 */
Stack stack_push(Stack s, mpz_t val) {
    if (s->size == s->cap) {
        size_t newcap, bytes;
        if (s->cap == 0)
            newcap = 8;
        else if (ckd_mul(&newcap, s->cap, (size_t) 2))
            rpn_fatal("stack too large");
        if (ckd_mul(&bytes, newcap, sizeof(mpz_t)))
            rpn_fatal("stack too large");

        mpz_t *grown = realloc(s->vals, bytes);
        if (!grown)
            rpn_fatal("memory allocation error");
        s->vals = grown;
        s->cap = newcap;
    }

    mpz_init_set(s->vals[s->size], val);
    s->size++;
    return s;
}

/**
 * Take the value at the top of the stack
 * @param s Stack to take the value from
 * @param val pointer to store the taken value (initialized by this call)
 * @return the same stack, without the former top value
 */
Stack stack_pop(Stack s, mpz_t *val) {
    if (s->size > 0) {
        s->size--;
        mpz_init_set(*val, s->vals[s->size]);
        mpz_clear(s->vals[s->size]);
    }

    return s;
}

/**
 * Check if the stack has at least n elements
 * @return true if the is n or more elements in the stack
 */
bool stack_hasAtLeast(Stack s, int n) {
    return n <= 0 || s->size >= (size_t) n;
}

/**
 * Remove all values from the stack, keeping the (now empty) stack usable
 * @param s The stack to clear
 * @return the same stack, empty
 */
Stack stack_clear(Stack s) {
    for (size_t i = 0; i < s->size; i++)
        mpz_clear(s->vals[i]);
    s->size = 0;
    return s;
}

/**
 * Free all the space occupied by the stack
 * @param s The stack to free
 * @return NULL
 */
Stack stack_delete(Stack s) {
    if (!s)
        return NULL;

    s = stack_clear(s);
    free(s->vals);
    free(s);
    return NULL;
}
