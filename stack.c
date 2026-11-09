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
 * Double the capacity of a heap array that has run full, with the byte size
 * checked against overflow (initial capacity 8). Shared by the stack and
 * register arrays.
 * @param ptr The array pointer (may be NULL)
 * @param cap Current capacity, updated to the new capacity
 * @param elem_size Size of one element in bytes
 * @return The grown array (realloc may have freed the old pointer)
 */
void *checked_grow(void *ptr, size_t *cap, size_t elem_size) {
    size_t newcap, bytes;
    if (*cap == 0)
        newcap = 8;
    else if (ckd_mul(&newcap, *cap, (size_t) 2))
        rpn_fatal("array too large");
    if (ckd_mul(&bytes, newcap, elem_size))
        rpn_fatal("array too large");

    void *grown = realloc(ptr, bytes);
    if (!grown)
        rpn_fatal("memory allocation error");
    *cap = newcap;
    return grown;
}

/**
 * Grow the backing array if it is full and return the slot for the next
 * value. Reallocating an array of Value only moves the handles; the limbs
 * and string buffers they point to are allocated separately and stay valid.
 * @param s Stack to grow
 * @return Pointer to the free slot at the top of the stack
 */
static Value *stack_slot(Stack s) {
    if (s->size == s->cap)
        s->vals = checked_grow(s->vals, &s->cap, sizeof(Value));

    return &s->vals[s->size];
}

/**
 * Append a number at the top of a stack
 * @param s Stack to be completed
 * @param val value to add
 * @return the same stack, with the new value on top
 */
Stack stack_push_num(Stack s, const mpz_t val) {
    return stack_push_num_scale(s, val, 0);
}

/**
 * Append a number with a scale at the top of a stack (value = val / 10^scale)
 * @param s Stack to be completed
 * @param val digits of the value to add
 * @param scale number of fractional digits
 * @return the same stack, with the new value on top
 */
Stack stack_push_num_scale(Stack s, const mpz_t val, long scale) {
    Value *slot = stack_slot(s);
    slot->kind = VAL_NUM;
    slot->scale = scale;
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
    slot->scale = 0;
    s->size++;
    return s;
}

/**
 * Append a deep copy of a value at the top of a stack. The value is taken by
 * value on purpose: passing *stack_top(s) would alias the backing array,
 * which stack_slot() may realloc (freeing the old block) before the copy,
 * leaving v->num dangling. A by-value parameter copies the handle at the
 * call site, before any realloc.
 * @param s Stack to be completed
 * @param v Value to copy (number or string)
 * @return the same stack, with the new value on top
 */
Stack stack_push_value(Stack s, Value v) {
    if (v.kind == VAL_NUM)
        return stack_push_num_scale(s, v.num, v.scale);
    return stack_push_str(s, v.str);
}

/**
 * Take the value at the top of the stack. Fatal if the stack is empty:
 * callers must guard with stack_hasAtLeast so a forgotten guard fails
 * loudly here instead of leaving *out uninitialized.
 * @param s Stack to take the value from
 * @param out pointer to store the taken value (ownership moves to the caller)
 * @return the same stack, without the former top value
 */
Stack stack_pop(Stack s, Value *out) {
    if (s->size == 0)
        rpn_fatal("stack underflow");
    s->size--;
    *out = s->vals[s->size];
    return s;
}

/**
 * Move the top value of one stack onto another: pop it from `from` and push
 * a copy onto `to`. Callers must guard `from` with stack_hasAtLeast.
 * @param from Stack to take the value from
 * @param to Stack to push the value onto
 */
void stack_move_top(Stack from, Stack to) {
    Value v;
    from = stack_pop(from, &v);
    to = stack_push_value(to, v);
    value_clear(&v);
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
 * Rotate the top m values of the stack by one position (m is clamped to the
 * stack size): with reverse=false the bottom of the rotated segment moves to
 * the top, with reverse=true the top moves to the bottom. Fewer than two
 * values to rotate is a no-op. Values are moved bitwise, so each keeps its
 * slot's ownership of the limbs or string buffer it points to.
 * @param s Stack to rotate
 * @param m Number of top values to rotate
 * @param reverse Direction of the rotation
 */
void stack_rotate_top(Stack s, size_t m, bool reverse) {
    if (m > s->size)
        m = s->size;
    if (m < 2)
        return;

    Value *seg = &s->vals[s->size - m];
    if (!reverse) {
        Value tmp = seg[0];
        memmove(seg, seg + 1, (m - 1) * sizeof(Value));
        seg[m - 1] = tmp;
    } else {
        Value tmp = seg[m - 1];
        memmove(seg + 1, seg, (m - 1) * sizeof(Value));
        seg[0] = tmp;
    }
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
