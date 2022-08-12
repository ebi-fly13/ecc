CFLAGS=-std=c11 -g -static

ecc: ecc.c

test: ecc
		./test.sh

clean:
		rm -f ecc *.o *~ tmp*

.PHONY: test clean