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

#ifndef STACK_H
#define STACK_H

#include <stddef.h>

#include <gmp.h>

/** The kind of a stack value */
typedef enum ValueKind {
    VAL_NUM,  /**< arbitrary-precision integer */
    VAL_STR   /**< string */
} ValueKind;

/**
 * A stack value: either an arbitrary-precision integer or a string.
 * Only the member matching kind is valid.
 */
typedef struct Value {
    ValueKind kind;
    union {
        mpz_t num;   /**< integer, when kind == VAL_NUM */
        char *str;   /**< heap-allocated NUL-terminated string, when kind == VAL_STR */
    };
} Value;

/**
 * A stack of values backed by a dynamically growing array:
 * vals[0] is the bottom of the stack, vals[size-1] the top.
 */
typedef struct StructStack {
    Value *vals;
    size_t size;
    size_t cap;
} StructStack;

typedef StructStack* Stack;

/** The top value of the stack (only valid when the stack is non-empty) */
static inline Value *stack_top(Stack s) { return &s->vals[s->size - 1]; }

/** The value just below the top (only valid with at least two values) */
static inline Value *stack_second(Stack s) { return &s->vals[s->size - 2]; }

Stack stack_init();

/** Release the resources held by a value (mpz_clear or free) */
void value_clear(Value *v);

[[nodiscard]] Stack stack_push_num(Stack, const mpz_t);

[[nodiscard]] Stack stack_push_str(Stack, const char *);

/** Push a deep copy of a value of either kind. The value is taken by value
 * so a caller may safely pass *stack_top(s): the struct copy happens before
 * the backing array grows, and only the separately-allocated limbs/buffers
 * are shared with the copy. */
[[nodiscard]] Stack stack_push_value(Stack, Value);

[[nodiscard]] Stack stack_pop(Stack, Value *);

bool stack_hasAtLeast(Stack, int);

[[nodiscard]] Stack stack_clear(Stack);

Stack stack_delete(Stack);

#endif
