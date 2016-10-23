#ifndef STACK_H
#define STACK_H

typedef struct StructStack {
    int val;
    struct StructStack* below;
} StructStack;

typedef StructStack* Stack;

Stack stack_init();

Stack stack_push(Stack, int);

Stack stack_pop(Stack, int*);

short stack_isEmpty(Stack);

void stack_delete(Stack);

#endif

