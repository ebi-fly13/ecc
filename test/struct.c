#include "test.h"

int assert();
int printf();

int main() {
    ASSERT(1, ({ struct {int a; int b;} x; x.a=1; x.b=2; x.a; }));
    ASSERT(2, ({ struct {int a; int b;} x; x.a=1; x.b=2; x.b; }));
    ASSERT(1, ({ struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; x.a; }));
    ASSERT(2, ({ struct {char a; int b; char c;} x; x.b=1; x.b=2; x.c=3; x.b; }));
    ASSERT(3, ({ struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; x.c; }));

    ASSERT(8, ({ struct t {int a; int b;} x; struct t y; sizeof(y); }));
    ASSERT(2, ({ struct t {char a[2];}; { struct t {char a[4];}; } struct t y; sizeof(y); }));
    ASSERT(3, ({ struct t {int x;}; int t=1; struct t y; y.x=2; t+y.x; }));

    ASSERT(3, ({ struct t {char a;} x; struct t *y = &x; x.a=3; y->a; }));
    ASSERT(3, ({ struct t {char a;} x; struct t *y = &x; y->a=3; x.a; }));

    ASSERT(9, ({struct {char a; long b;} x; sizeof(x); }));
    ASSERT(10, ({struct {short a; long b;} x; sizeof(x); }));

    ASSERT(8, ({ struct foo *bar; sizeof(bar); }));
    ASSERT(4, ({ struct T *foo; struct T {int x;}; sizeof(struct T); }));
    ASSERT(1, ({ struct T { struct T *next; int x; } a; struct T b; b.x=1; a.next=&b; a.next->x; }));
    ASSERT(4, ({ typedef struct T T; struct T { int x; }; sizeof(T); }));

    ASSERT(4, sizeof(struct { int x, y[]; }));

    printf("OK\n");
    return 0;
}  