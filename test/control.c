#include "test.h"

int assert();
int printf();

/*
 * This is a block comment.
 */

int main() {
  ASSERT(3, ({ int x; if (0) x=2; else x=3; x; }));
  ASSERT(3, ({ int x; if (1-1) x=2; else x=3; x; }));
  ASSERT(2, ({ int x; if (1) x=2; else x=3; x; }));
  ASSERT(2, ({ int x; if (2-1) x=2; else x=3; x; }));

  ASSERT(55, ({ int i; int j; j = 0; for (i=0; i<=10; i=i+1) j=i+j; j; }));

  ASSERT(10, ({ int i; i = 0; while(i<10) i=i+1; i; }));

  ASSERT(3, ({ 1; {2;} 3; }));
  ASSERT(5, ({ ;;; 5; }));

  ASSERT(55, ({ int i; i = 0; int j; j = 0; while(i<=10) {j=i+j; i=i+1;} j; }));

  return 0;
}