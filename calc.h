#ifndef CALC_H
#define CALC_H

#include "stack.h"

int calc_parse(Stack s, const char* string);

void calc_printMessageCode(int code);

#endif

