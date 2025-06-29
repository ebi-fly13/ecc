#include "test.h"

int assert();
int printf();

extern int ext1;
extern int *ext2;
extern int a;
int a = 1;
int b = 2;
extern int b;

int main() {
    ASSERT(5, ext1);
    ASSERT(5, *ext2);
    extern int ext3;
    ASSERT(7, ext3);

    int ext_fn1(int x);
    ASSERT(5, ext_fn1(5));

    extern int ext_fn2(int x);
    ASSERT(8, ext_fn2(8));

    ASSERT(1, a);
    ASSERT(2, b);

    printf("OK\n");
    return 0;
}