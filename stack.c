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
#include <string.h>
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
 * Release the resources held by a value
 * @param v The value to release (mpz_clear for a number, free for a string)
 */
void value_clear(Value *v) {
    if (v->kind == VAL_NUM)
        mpz_clear(v->num);
    else
        free(v->str);
}

/**
 * Grow the backing array if it is full and return the slot for the next
 * value, doubling the capacity with the byte size checked against overflow.
 * Reallocating an array of Value only moves the handles; the limbs and
 * string buffers they point to are allocated separately and stay valid.
 * @param s Stack to grow
 * @return Pointer to the free slot at the top of the stack
 */
static Value *stack_slot(Stack s) {
    if (s->size == s->cap) {
        size_t newcap, bytes;
        if (s->cap == 0)
            newcap = 8;
        else if (ckd_mul(&newcap, s->cap, (size_t) 2))
            rpn_fatal("stack too large");
        if (ckd_mul(&bytes, newcap, sizeof(Value)))
            rpn_fatal("stack too large");

        Value *grown = realloc(s->vals, bytes);
        if (!grown)
            rpn_fatal("memory allocation error");
        s->vals = grown;
        s->cap = newcap;
    }

    return &s->vals[s->size];
}

/**
 * Append a number at the top of a stack
 * @param s Stack to be completed
 * @param val value to add
 * @return the same stack, with the new value on top
 */
Stack stack_push_num(Stack s, const mpz_t val) {
    Value *slot = stack_slot(s);
    slot->kind = VAL_NUM;
    mpz_init_set(slot->num, val);
    s->size++;
    return s;
}

/**
 * Append a string at the top of a stack (the string is copied)
 * @param s Stack to be completed
 * @param str NUL-terminated string to add
 * @return the same stack, with the new value on top
 */
Stack stack_push_str(Stack s, const char *str) {
    Value *slot = stack_slot(s);
    size_t size;
    if (ckd_add(&size, strlen(str), (size_t) 1))
        rpn_fatal("string too large");

    slot->str = malloc(size);
    if (!slot->str)
        rpn_fatal("memory allocation error");
    memcpy(slot->str, str, size);  // copies the terminating NUL as well
    slot->kind = VAL_STR;
    s->size++;
    return s;
}

/**
 * Append a deep copy of a value at the top of a stack
 * @param s Stack to be completed
 * @param v Value to copy (number or string)
 * @return the same stack, with the new value on top
 */
Stack stack_push_value(Stack s, const Value *v) {
    if (v->kind == VAL_NUM)
        return stack_push_num(s, v->num);
    return stack_push_str(s, v->str);
}

/**
 * Take the value at the top of the stack
 * @param s Stack to take the value from
 * @param out pointer to store the taken value (ownership moves to the caller)
 * @return the same stack, without the former top value
 */
Stack stack_pop(Stack s, Value *out) {
    if (s->size > 0) {
        s->size--;
        *out = s->vals[s->size];
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
        value_clear(&s->vals[i]);
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
