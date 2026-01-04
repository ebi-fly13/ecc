#include <stdio.h>
#include <stdlib.h>

#define ASSERT(x, y) assert(x, y, #y)

void assert(int expected, int actual, char *code);