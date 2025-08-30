int printf(char *fmt, ...);
void assert(int expected, int actual, char *code);

#

/* */ #

#include "include1.h"

#if 1
int a = 0;
#if 0
a = 1;
#endif
#endif
#if 0
int a = 1;
#if 1
a = 2;
#endif
#endif

int main() {
  assert(1, include1, "include1");
  assert(2, include2, "include2");
  assert(0, a, "a");
  printf("OK\n");
  return 0;
}