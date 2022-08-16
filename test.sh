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

assert 0 "0;"
assert 42 "42;"
assert 21 "5+20-4;"
assert 41 " 12 + 34 - 5 ;"
assert 47 "5+6*7;"
assert 15 "5*(9-6);"
assert 4 "(3+5)/2;"
assert 10 "+10;"
assert 10 "-10+20;"
assert 6 "-2 * (-3) + 4 * -0;"
assert 1 "4 >= 2;"
assert 0 "7 > 8;"
assert 1 "1 <= 2;"
assert 0 "1 > 2;"
assert 1 "10 == 10;"
assert 0 "10 == 3;"
assert 1 "10 != 5;"
assert 0 "5 != 5;"
assert 4 "a = 4; a;"
assert 5 "a = 2; z = 3; a+z;"
assert 10 "aaa = 1; b_a = 4; a = (aaa + b_a)*2; a;"
assert 14 "a = 3; b = 5*6 - 8; return a+b/2;"
assert 5 "return 5; return 8;"
assert 4 "if(1) return 4; return 5;"
assert 5 "if(0) return 4; return 5;"
assert 3 "a = 1; if(a) return 3; else return 4;"
assert 5 "a = 0; if(a) return 4; else return 5;"
assert 10 "a = 0; while(a < 10) a = a + 1; return a;"
assert 100 "a = 0; for(i = 0; i < 10; i = i + 1) a = a + 10; return a;"
assert 10 "a = 0; for(;a < 100;) a = a + 1; return a/10;"
assert 1 "a = 0; {a = a + 1;} return a;"
assert 10 "a = 0; b = 0; for(;a < 100;) { a = a + 10; b = b + 1; } return b;"
assert 1 "a = 1; ;;; return a;"

echo OK