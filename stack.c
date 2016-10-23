#include <stdio.h>
#include <stdlib.h>

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
 */
Stack stack_push(Stack s, int val) {
    Stack n = (Stack) malloc(sizeof(StructStack));

    if (!n) {
        puts("Memory allocation error\n");
        exit(1);
    }

    n->val = val;
    n->below = s;

    return n;
}

/**
 * Take the value at the top of the stack
 * @param s Stack to take the value from
 * @param val pointer to store the taken value
 */
Stack stack_pop(Stack s, int* val) {
    Stack below = NULL;

    if (s) {
        *val = s->val;
        below = s->below;
        free(s);
    }

    return below;
}

/**
 * Check if the stack is empty
 */
short stack_isEmpty(Stack s) {
    return s == NULL;
}

/**
 * Free all the space occupied by the stack
 */
void stack_delete(Stack s) {
    Stack below = NULL;

    while (s) {
        below = s->below;
        free(s);
        s = below;
    }
}

