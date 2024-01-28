#include "test.h"

int assert();
int printf();

int main() {
    ASSERT(3, ({ int x = 3; int *y = &x; *y; }));
    ASSERT(3, ({ int x = 2; int *y = &x; *y = 3; x; }));
    ASSERT(2, ({int x[3]; *x = 1; *(x + 1) = 2; *(x + 1); }));
    ASSERT(3, ({int x[3]; *x = 1; *(x + 1) = 2; int *p = x; *p + *(p + 1); }));
    ASSERT(4, ({ int x[3]; *x=3; x[1]=4; x[2]=5; *(x+1); }));
    ASSERT(5, ({ int x[3]; *x=3; x[1]=4; x[2]=5; *(x+2); }));
    ASSERT(5, ({ int x[3]; *x=3; x[1]=4; 2[x]=5; *(x+2); }));
    return 0;
}