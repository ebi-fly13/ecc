#include "test.h"

int assert();
int printf();

int main() {
    ASSERT(24, ({struct {int a; int b,c;} x; sizeof(x); }));
    return 0;
}