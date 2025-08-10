int printf(char *fmt, ...);
void assert(int expected, int actual, char *code);

#

/* */ #

#include "include1.h"

int main() {
  assert(1, include1, "include1");
  assert(2, include2, "include2");
  printf("OK\n");
  return 0;
}