CFLAGS=-std=c11 -g -static
SRCS= $(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:.c=.exe)

ecc: $(OBJS)
		$(CC) -o ecc $(OBJS) $(LDFLAGS)

$(OBJS): ecc.h

test/%.exe: ecc test/%.c
		$(CC) -o- -E -P -C test/$*.c > test/_.c
		./ecc test/_.c > test/$*.s
		$(CC) -static -o $@ test/$*.s -xc test/common
		rm test/_.c

test: $(TESTS)
		./test.sh
		for i in $^; do echo $$i; ./$$i || exit 1; echo; done

clean:
		rm -f ecc *.o *~ tmp*
		rm -f test/*.s test/*.o test/*.exe

.PHONY: test clean