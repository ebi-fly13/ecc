#!/bin/bash

assert() {
  expected="$1"
  input="$2"

  ./ecc "$input" > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 "int main() { return 0;}"
assert 42 "int main(){ return 42;}"
assert 21 "int main() { return 5+20-4;}"
assert 41 "int main() { return 12 + 34 - 5 ;}"
assert 47 "int main() { return 5+6*7;} "
assert 15 "int main() {5*(9-6);}"
assert 4 "int main () {(3+5)/2;}"
assert 10 "int main() {return+10;}"
assert 10 "int main() {return -10+20;}"
assert 6 "int main() { return -2 * (-3) + 4 * -0;}"
assert 1 "int main() { return 4 >= 2;}"
assert 0 "int main() { return 7 > 8;}"
assert 1 "int main() {return 1 <= 2;}"
assert 0 "int main() {return 1 > 2;}"
assert 1 "int main() { return 10 == 10;}"
assert 0 "int main() { return 10 == 3;}"
assert 1 "int main() { return 10 != 5;}"
assert 0 "int main() { return 5 != 5;}"
assert 4 "int main() { int a; a = 4; a;}"
assert 5 "int main() { int a; a = 2; int z; z = 3; a+z;}"
assert 10 "int main() { int aaa; aaa = 1; int b_a; b_a = 4; int a; a = (aaa + b_a)*2; a;}"
assert 14 "int main() { int a; a = 3;int b; b = 5*6 - 8; return a+b/2;}"
assert 5 "int main() { return 5; return 8;}"
assert 4 "int main() {if(1) return 4; return 5;}"
assert 5 "int main() {if(0) return 4; return 5;}"
assert 3 "int main() {int a; a = 1; if(a) return 3; else return 4;}"
assert 5 "int main() {int a; a = 0; if(a) return 4; else return 5;}"
assert 10 "int main() { int a; a = 0; while(a < 10) a = a + 1; return a;}"
assert 100 "int main() { int a; int i; a = 0; for(i = 0; i < 10; i = i + 1) a = a + 10; return a;}"
assert 10 "int main() {int a; a = 0; for(;a < 100;) a = a + 1; return a/10;}"
assert 1 "int main(){int a; a = 0; {a = a + 1;} return a;}"
assert 10 "int main() {int a; int b; a = 0; b = 0; for(;a < 100;) { a = a + 10; b = b + 1; } return b;}"
assert 1 "int main(){int a; a = 1; ;;; return a;}"
assert 3 "int ret3() {return 3;} int main() {return ret3();}"
assert 5 "int ret5(){return 5;} int main() {{ return ret5(); }}"
assert 21 " int add6(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }  int main() {return add6(1,2,3,4,5,6);}"
assert 66 " int add6( int a, int b, int c, int  d, int e, int f) { return a + b + c + d + e + f; }  int main(){{ return add6(1,2,add6(3,4,5,6,7,8),9,10,11); }}"
assert 136 " int add6( int a, int b, int c, int d,int e, int f) { return a + b + c + d + e + f; } int main(){ return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }"
assert 3 "int main() { int x; x = 3; int *y; y = &x; return *y; }"
assert 3 "int main() { int x; int *y; y = &x; *y = 3; return x;}"
assert 1 "int main() { int x; int y; return &x - &y; }"
assert 2 "int main() { int x; x = 2; int y; return *(&y + 1); }"
assert 8 "int main() { int x; return sizeof(x); }"
assert 8 "int main() { int *x; return sizeof(x); }"
assert 40 "int main() { int x[5]; return sizeof(x); }"
assert 2 "int main() {int x[3]; *x = 1; *(x+1) = 2; return *(x+1); }"
assert 3 "int main() {int x[3]; *x = 1; *(x+1) = 2; int *p; p = x; return *p + *(p+1); }"
assert 3 "int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *x; }"
assert 4 "int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+1); }"
assert 5 "int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); }"
assert 5 "int main() { int x[3]; *x=3; x[1]=4; 2[x]=5; return *(x+2); }"
assert 1 "int a; int main() { a = 1; return a;}"
assert 3 "int a; int change3() {a = 3; return 1;} int main() { change3() return a;}"

echo OK