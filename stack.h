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

/**
 * A stack of arbitrary-precision integers backed by a dynamically growing
 * array: vals[0] is the bottom of the stack, vals[size-1] the top.
 */
typedef struct StructStack {
    mpz_t *vals;
    size_t size;
    size_t cap;
} StructStack;

typedef StructStack* Stack;

/** The top element of the stack (only valid when the stack is non-empty) */
#define STACK_TOP(s) ((s)->vals[(s)->size - 1])

/** The element just below the top (only valid with at least two elements) */
#define STACK_SECOND(s) ((s)->vals[(s)->size - 2])

Stack stack_init();

[[nodiscard]] Stack stack_push(Stack, mpz_t);

[[nodiscard]] Stack stack_pop(Stack, mpz_t*);

bool stack_hasAtLeast(Stack, int);

[[nodiscard]] Stack stack_clear(Stack);

[[nodiscard]] Stack stack_delete(Stack);

#endif
