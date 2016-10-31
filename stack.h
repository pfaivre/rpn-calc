/**
MIT License

Copyright (c) 2016 Pierre Faivre

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

#include <stdbool.h>

#include <gmp.h>

/**
 * An implementation of a stack as a FILO linked list.
 * A stack can be accessed by a pointer to its higest element.
 */
typedef struct StructStack {
    mpz_t val;
    struct StructStack* below;
} StructStack;

typedef StructStack* Stack;

Stack stack_init();

Stack stack_push(Stack, mpz_t);

Stack stack_pop(Stack, mpz_t*);

bool stack_isEmpty(Stack);

bool stack_hasAtLeast(Stack, int);

Stack stack_delete(Stack);

#endif

