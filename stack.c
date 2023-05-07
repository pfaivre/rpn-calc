/**
MIT License

Copyright (c) 2016-2023 Pierre Faivre

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
#include <stdbool.h>

#include <gmp.h>

#include "stack.h"

/**
 * Creates a new empty Stack
 */
Stack stack_init() {
    return NULL; // Amazing
}

/**
 * Append a value at the top of a stack
 * @param s Stack to be completed
 * @param val value to add
 * @return the new top of the stack
 */
Stack stack_push(Stack s, mpz_t val) {
    Stack n = (Stack) malloc(sizeof(StructStack));

    if (!n) {
        fputs("Memory allocation error\n", stderr);
        exit(1);
    }

    mpz_init_set(n->val, val);
    n->below = s;

    return n;
}

/**
 * Take the value at the top of the stack
 * @param s Stack to take the value from
 * @param val pointer to store the taken value
 * @return the new top of the stack
 */
Stack stack_pop(Stack s, mpz_t *val) {
    Stack below = NULL;

    if (s) {
        mpz_init_set(*val, s->val);
        below = s->below;
        mpz_clear(s->val);
        free(s);
    }

    return below;
}

/**
 * Check if the stack is empty
 * @return true if there is no more elements in the stack
 */
bool stack_isEmpty(Stack s) {
    return s == NULL;
}

/**
 * Check if the stack has at least n elements
 * @return true if the is n or more elements in the stack
 */
bool stack_hasAtLeast(Stack s, int n) {
    while (s != NULL && (n-- > 0)) {
        s = s->below;
    }

    return n <= 0;
}

/**
 * Free all the space occupied by the stack
 * @param s The stack to free. Note that all of the chained elements will be
 *          freed as well
 */
Stack stack_delete(Stack s) {
    Stack below = NULL;

    while (s) {
        below = s->below;
        mpz_clear(s->val);
        free(s);
        s = below;
    }

    return s;
}

