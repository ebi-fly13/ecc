#include <stdio.h>
#include <string.h>

void assert(int expected, int actual, char *code) {
    if (expected == actual) {
        printf("%s => %d\n", code, actual);
    } else {
        printf("%s => %d expected but got %d\n", code, expected, actual);
        exit(1);
    }
}
char g3 = 3;
short g4 = 4;
int g5 = 5;
long g6 = 6;
int g7[3] = {0, 1, 2};
struct {
    char a;
    int b;
} g11[2] = {{1, 2}, {3, 4}};
struct {
    int a[2];
} g12[2] = {{{1, 2}}};
union {
    int a;
    char b[8];
} g13[2] = {0x01020304, 0x05060708};
char g17[] = "foobar";
char g18[10] = "foobar";
char g19[3] = "foobar";
char *g20 = g17 + 0;
char *g21 = g17 + 3;
char *g22 = &g17 - 3;
char *g23[] = {g17 + 0, g17 + 3, g17 - 3};
int g24 = 3;
int *g25 = &g24;
int g26[3] = {1, 2, 3};
int *g27 = g26 + 1;
char *g28 = &g11[1].a;
long g29 = (long)(long)g26;
struct {
    struct {
        int a[3];
    } a;
} g30 = {{{1, 2, 3}}};
int *g31 = g30.a.a;
struct {
    int a[2];
} g40[2] = {{1, 2}, 3, 4};
struct {
    int a[2];
} g41[2] = {1, 2, 3, 4};
char g43[][4] = {'f', 'o', 'o', 0, 'b', 'a', 'r', 0};
int main() {
    assert(1, ({
               int x[3] = {1, 2, 3};
               x[0];
           }),
           "({ int x[3]={1,2,3}; x[0]; })");
    assert(2, ({
               int x[3] = {1, 2, 3};
               x[1];
           }),
           "({ int x[3]={1,2,3}; x[1]; })");
    assert(3, ({
               int x[3] = {1, 2, 3};
               x[2];
           }),
           "({ int x[3]={1,2,3}; x[2]; })");
    assert(3, ({
               int x[3] = {1, 2, 3};
               x[2];
           }),
           "({ int x[3]={1,2,3}; x[2]; })");
    assert(2, ({
               int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
               x[0][1];
           }),
           "({ int x[2][3]={{1,2,3},{4,5,6}}; x[0][1]; })");
    assert(4, ({
               int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
               x[1][0];
           }),
           "({ int x[2][3]={{1,2,3},{4,5,6}}; x[1][0]; })");
    assert(6, ({
               int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
               x[1][2];
           }),
           "({ int x[2][3]={{1,2,3},{4,5,6}}; x[1][2]; })");
    assert(0, ({
               int x[3] = {};
               x[0];
           }),
           "({ int x[3]={}; x[0]; })");
    assert(0, ({
               int x[3] = {};
               x[1];
           }),
           "({ int x[3]={}; x[1]; })");
    assert(0, ({
               int x[3] = {};
               x[2];
           }),
           "({ int x[3]={}; x[2]; })");
    assert(2, ({
               int x[2][3] = {{1, 2}};
               x[0][1];
           }),
           "({ int x[2][3]={{1,2}}; x[0][1]; })");
    assert(0, ({
               int x[2][3] = {{1, 2}};
               x[1][0];
           }),
           "({ int x[2][3]={{1,2}}; x[1][0]; })");
    assert(0, ({
               int x[2][3] = {{1, 2}};
               x[1][2];
           }),
           "({ int x[2][3]={{1,2}}; x[1][2]; })");
    assert(1, ({
               int x[1] = {1, {2, {3}}};
               x[0];
           }),
           "({int x[1] = {1, {2, {3}}}; x[0]; })");
    assert('a', ({
               char x[4] = "abc";
               x[0];
           }),
           "({ char x[4]=\"abc\"; x[0]; })");
    assert('c', ({
               char x[4] = "abc";
               x[2];
           }),
           "({ char x[4]=\"abc\"; x[2]; })");
    assert(0, ({
               char x[4] = "abc";
               x[3];
           }),
           "({ char x[4]=\"abc\"; x[3]; })");
    assert('a', ({
               char x[2][4] = {"abc", "def"};
               x[0][0];
           }),
           "({ char x[2][4]={\"abc\",\"def\"}; x[0][0]; })");
    assert(0, ({
               char x[2][4] = {"abc", "def"};
               x[0][3];
           }),
           "({ char x[2][4]={\"abc\",\"def\"}; x[0][3]; })");
    assert('d', ({
               char x[2][4] = {"abc", "def"};
               x[1][0];
           }),
           "({ char x[2][4]={\"abc\",\"def\"}; x[1][0]; })");
    assert('f', ({
               char x[2][4] = {"abc", "def"};
               x[1][2];
           }),
           "({ char x[2][4]={\"abc\",\"def\"}; x[1][2]; })");
    assert(4, ({
               int x[] = {1, 2, 3, 4};
               x[3];
           }),
           "({ int x[]={1,2,3,4}; x[3]; })");
    assert(16, ({
               int x[] = {1, 2, 3, 4};
               sizeof(x);
           }),
           "({ int x[]={1,2,3,4}; sizeof(x); })");
    assert(4, ({
               char x[] = "foo";
               sizeof(x);
           }),
           "({ char x[]=\"foo\"; sizeof(x); })");
    assert(4, ({
               typedef char T[];
               T x = "foo";
               T y = "x";
               sizeof(x);
           }),
           "({ typedef char T[]; T x=\"foo\"; T y=\"x\"; sizeof(x); })");
    assert(2, ({
               typedef char T[];
               T x = "foo";
               T y = "x";
               sizeof(y);
           }),
           "({ typedef char T[]; T x=\"foo\"; T y=\"x\"; sizeof(y); })");
    assert(2, ({
               typedef char T[];
               T x = "x";
               T y = "foo";
               sizeof(x);
           }),
           "({ typedef char T[]; T x=\"x\"; T y=\"foo\"; sizeof(x); })");
    assert(4, ({
               typedef char T[];
               T x = "x";
               T y = "foo";
               sizeof(y);
           }),
           "({ typedef char T[]; T x=\"x\"; T y=\"foo\"; sizeof(y); })");
    assert(1, ({
               struct {
                   int a;
                   int b;
                   int c;
               } x = {1, 2, 3};
               x.a;
           }),
           "({ struct {int a; int b; int c;} x={1,2,3}; x.a; })");
    assert(2, ({
               struct {
                   int a;
                   int b;
                   int c;
               } x = {1, 2, 3};
               x.b;
           }),
           "({ struct {int a; int b; int c;} x={1,2,3}; x.b; })");
    assert(3, ({
               struct {
                   int a;
                   int b;
                   int c;
               } x = {1, 2, 3};
               x.c;
           }),
           "({ struct {int a; int b; int c;} x={1,2,3}; x.c; })");
    assert(1, ({
               struct {
                   int a;
                   int b;
                   int c;
               } x = {1};
               x.a;
           }),
           "({ struct {int a; int b; int c;} x={1}; x.a; })");
    assert(0, ({
               struct {
                   int a;
                   int b;
                   int c;
               } x = {1};
               x.b;
           }),
           "({ struct {int a; int b; int c;} x={1}; x.b; })");
    assert(0, ({
               struct {
                   int a;
                   int b;
                   int c;
               } x = {1};
               x.c;
           }),
           "({ struct {int a; int b; int c;} x={1}; x.c; })");
    assert(1, ({
               struct {
                   int a;
                   int b;
               } x[2] = {{1, 2}, {3, 4}};
               x[0].a;
           }),
           "({ struct {int a; int b;} x[2]={{1,2},{3,4}}; x[0].a; })");
    assert(2, ({
               struct {
                   int a;
                   int b;
               } x[2] = {{1, 2}, {3, 4}};
               x[0].b;
           }),
           "({ struct {int a; int b;} x[2]={{1,2},{3,4}}; x[0].b; })");
    assert(3, ({
               struct {
                   int a;
                   int b;
               } x[2] = {{1, 2}, {3, 4}};
               x[1].a;
           }),
           "({ struct {int a; int b;} x[2]={{1,2},{3,4}}; x[1].a; })");
    assert(4, ({
               struct {
                   int a;
                   int b;
               } x[2] = {{1, 2}, {3, 4}};
               x[1].b;
           }),
           "({ struct {int a; int b;} x[2]={{1,2},{3,4}}; x[1].b; })");
    assert(0, ({
               struct {
                   int a;
                   int b;
               } x[2] = {{1, 2}};
               x[1].b;
           }),
           "({ struct {int a; int b;} x[2]={{1,2}}; x[1].b; })");
    assert(0, ({
               struct {
                   int a;
                   int b;
               } x = {};
               x.a;
           }),
           "({ struct {int a; int b;} x={}; x.a; })");
    assert(0, ({
               struct {
                   int a;
                   int b;
               } x = {};
               x.b;
           }),
           "({ struct {int a; int b;} x={}; x.b; })");
    assert(5, ({
               typedef struct {
                   int a, b, c, d, e, f;
               } T;
               T x = {1, 2, 3, 4, 5, 6};
               T y;
               y = x;
               y.e;
           }),
           "({ typedef struct {int a,b,c,d,e,f;} T; T x={1,2,3,4,5,6}; T y; "
           "y=x; y.e; })");
    assert(2, ({
               typedef struct {
                   int a, b;
               } T;
               T x = {1, 2};
               T y, z;
               z = y = x;
               z.b;
           }),
           "({ typedef struct {int a,b;} T; T x={1,2}; T y, z; z=y=x; z.b; })");
    assert(1, ({
               typedef struct {
                   int a, b;
               } T;
               T x = {1, 2};
               T y = x;
               y.a;
           }),
           "({ typedef struct {int a, b;} T; T x = {1, 2}; T y = x; y.a;})");
    assert(4, ({
               union {
                   int a;
                   char b[4];
               } x = {0x01020304};
               x.b[0];
           }),
           "({ union { int a; char b[4]; } x={0x01020304}; x.b[0]; })");
    assert(3, ({
               union {
                   int a;
                   char b[4];
               } x = {0x01020304};
               x.b[1];
           }),
           "({ union { int a; char b[4]; } x={0x01020304}; x.b[1]; })");
    assert(0x01020304, ({
               union {
                   struct {
                       char a, b, c, d;
                   } e;
                   int f;
               } x = {{4, 3, 2, 1}};
               x.f;
           }),
           "({ union { struct { char a,b,c,d; } e; int f; } x={{4,3,2,1}}; "
           "x.f; })");
    assert(3, g3, "g3");
    assert(4, g4, "g4");
    assert(5, g5, "g5");
    assert(6, g6, "g6");
    assert(0, g7[0], "g7[0]");
    assert(1, g7[1], "g7[1]");
    assert(2, g7[2], "g7[2]");
    assert(1, g11[0].a, "g11[0].a");
    assert(2, g11[0].b, "g11[0].b");
    assert(3, g11[1].a, "g11[1].a");
    assert(4, g11[1].b, "g11[1].b");
    assert(1, g12[0].a[0], "g12[0].a[0]");
    assert(2, g12[0].a[1], "g12[0].a[1]");
    assert(0, g12[1].a[0], "g12[1].a[0]");
    assert(0, g12[1].a[1], "g12[1].a[1]");
    assert(4, g13[0].b[0], "g13[0].b[0]");
    assert(3, g13[0].b[1], "g13[0].b[1]");
    assert(8, g13[1].b[0], "g13[1].b[0]");
    assert(7, g13[1].b[1], "g13[1].b[1]");
    assert(4, g13[0].b[0], "g13[0].b[0]");
    assert(3, g13[0].b[1], "g13[0].b[1]");
    assert(8, g13[1].b[0], "g13[1].b[0]");
    assert(7, g13[1].b[1], "g13[1].b[1]");
    assert(7, sizeof(g17), "sizeof(g17)");
    assert(10, sizeof(g18), "sizeof(g18)");
    assert(3, sizeof(g19), "sizeof(g19)");
    assert(0, memcmp(g17, "foobar", 7), "memcmp(g17, \"foobar\", 7)");
    assert(0, memcmp(g18, "foobar\0\0\0", 10),
           "memcmp(g18, \"foobar\\0\\0\\0\", 10)");
    assert(0, memcmp(g19, "foo", 3), "memcmp(g19, \"foo\", 3)");
    assert(0, strcmp(g20, "foobar"), "strcmp(g20, \"foobar\")");
    assert(0, strcmp(g21, "bar"), "strcmp(g21, \"bar\")");
    assert(0, strcmp(g23[0], "foobar"), "strcmp(g23[0], \"foobar\")");
    assert(0, strcmp(g23[1], "bar"), "strcmp(g23[1], \"bar\")");
    assert(0, strcmp(g23[2] + 3, "foobar"), "strcmp(g23[2]+3, \"foobar\")");
    assert(3, g24, "g24");
    assert(3, *g25, "*g25");
    assert(2, *g27, "*g27");
    assert(3, *g28, "*g28");
    assert(1, *(int *)g29, "*(int *)g29");
    assert(1, g31[0], "g31[0]");
    assert(2, g31[1], "g31[1]");
    assert(3, g31[2], "g31[2]");
    assert(1, g40[0].a[0], "g40[0].a[0]");
    assert(2, g40[0].a[1], "g40[0].a[1]");
    assert(3, g40[1].a[0], "g40[1].a[0]");
    assert(4, g40[1].a[1], "g40[1].a[1]");
    assert(1, g41[0].a[0], "g41[0].a[0]");
    assert(2, g41[0].a[1], "g41[0].a[1]");
    assert(3, g41[1].a[0], "g41[1].a[0]");
    assert(4, g41[1].a[1], "g41[1].a[1]");
    assert(0, ({
               int x[2][3] = {0, 1, 2, 3, 4, 5};
               x[0][0];
           }),
           "({ int x[2][3]={0,1,2,3,4,5}; x[0][0]; })");
    assert(3, ({
               int x[2][3] = {0, 1, 2, 3, 4, 5};
               x[1][0];
           }),
           "({ int x[2][3]={0,1,2,3,4,5}; x[1][0]; })");
    assert(0, ({
               struct {
                   int a;
                   int b;
               } x[2] = {0, 1, 2, 3};
               x[0].a;
           }),
           "({ struct {int a; int b;} x[2]={0,1,2,3}; x[0].a; })");
    assert(2, ({
               struct {
                   int a;
                   int b;
               } x[2] = {0, 1, 2, 3};
               x[1].a;
           }),
           "({ struct {int a; int b;} x[2]={0,1,2,3}; x[1].a; })");
    assert(0, strcmp(g43[0], "foo"), "strcmp(g43[0], \"foo\")");
    assert(0, strcmp(g43[1], "bar"), "strcmp(g43[1], \"bar\")");
    printf("OK\n");

    printf("%d\n", strcmp(g22 + 3, "foobar"));
    return 0;
}
