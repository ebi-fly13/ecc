#include "test.h"

int assert();
int printf();
int ret3();
int addx(int *, int);

int param_decay(int x[]) { 
    return x[0];
}

static int static_f(void) {
    return 3;
}

void f(void) {
    return;
}

int ret3() {
    return 3;
    return 5;
}

int add2(int x, int y) {
    return x + y;
}

int sub2(int x, int y) {
    return x - y;
}

int add6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

int addx(int *x, int y) {
    return *x + y;
}

int sub_char(char a, char b, char c) {
    return a - b - c;
}

int fib(int x) {
    if (x <= 1) return 1;
    return fib(x - 1) + fib(x - 2);
}

int sub_long(long a, long b, long c) {
    return a - b - c;
}

int sub_short(short a, short b, short c) {
    return a - b - c;
}

void swap(int *a, int *b) {
    int c = *a;
    *a = *b;
    *b = c;
    return;
}

int static_variable() {
    static int a = 0;
    a++;
    return a;
}

int counter() {
  static int i;
  static int j = 1+1;
  return i++ + j++;
}

_Bool true_fn() { return 513; }
_Bool false_fn() { return 512; }
char char_fn() { return (2 << 8) + 3; }
short short_fn() { return (2 << 16) + 5; }

int add_all(int n, ...);

int main() {
    ASSERT(3, ret3());
    ASSERT(8, add2(3, 5));
    ASSERT(2, sub2(5, 3));
    ASSERT(21, add6(1, 2, 3, 4, 5, 6));
    ASSERT(66, add6(1, 2, add6(3, 4, 5, 6, 7, 8), 9, 10, 11));
    ASSERT(136, add6(1, 2, add6(3, add6(4, 5, 6, 7, 8, 9), 10, 11, 12, 13), 14,
                     15, 16));

    ASSERT(7, add2(3, 4));
    ASSERT(1, sub2(4, 3));

    ASSERT(55, fib(9));

    ASSERT(1, ({ sub_char(7, 3, 3); }));

    ASSERT(1, sub_long(7, 3, 3));

    ASSERT(1, sub_short(7, 3, 3));

    ASSERT(3, ({ int a = 2, b = 3; swap(&a, &b); a; }));

    ASSERT(3, static_f());

    ASSERT(3, ({ int x[2]; x[0] = 3; param_decay(x); }));

    f();

    ASSERT(1, static_variable());
    ASSERT(2, static_variable());

    ASSERT(2, counter());
    ASSERT(4, counter());
    ASSERT(6, counter());

    ASSERT(1, true_fn());
    ASSERT(0, false_fn());
    ASSERT(3, char_fn());
    ASSERT(5, short_fn());

    ASSERT(15, add_all(5,1,2,3,4,5));
    ASSERT(5, add_all(4,1,2,3,-1));

    printf("OK\n");
    return 0;
}