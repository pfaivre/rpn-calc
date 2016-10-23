#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "stack.h"
#include "utils.h"


int main(int argc, char** argv) {
    char* input = NULL;

    input = bufferedInput();

    if (input)
        puts(input);
        free(input);

    return 0;
}

