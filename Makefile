CFLAGS=-std=c11 -g -static
SRCS= $(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:.c=.exe)

ecc: $(OBJS)
		$(CC) -o ecc $(OBJS) $(LDFLAGS)

$(OBJS): ecc.h

test/macro.exe: ecc test/macro.c
		./ecc test/macro.c -S -o test/macro.s
		$(CC) -o $@ test/macro.s -xc test/common

test/macro2.exe: ecc test/macro.c
		./ecc test/macro.c -E | ./ecc -o test/macro2.s - -S
		$(CC) -o $@ test/macro2.s -xc test/common
		./test/macro2.exe

test/%.exe: ecc test/%.c
		./ecc -o test/$*.s test/$*.c -S
		$(CC) -static -o $@ test/$*.s -xc test/common

test: $(TESTS)
		for i in $^; do echo $$i; ./$$i || exit 1; echo; done

# Stage 2

stage2/ecc: $(OBJS:%=stage2/%)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

stage2/%.s: ecc self.py %.c
	mkdir -p stage2
	python3 self.py ecc.h $*.c > stage2/$*.c
	./ecc stage2/$*.c -S -o stage2/$*.s

stage2/test/macro.exe: stage2/ecc test/macro.c
	mkdir -p stage2/test
	./stage2/ecc test/macro.c -o stage2/test/macro.s -S
	$(CC) -o $@ stage2/test/macro.s -xc test/common

stage2/test/macro2.exe: stage2/ecc test/macro.c
	mkdir -p stage2/test
	./stage2/ecc test/macro.c -E | ./stage2/ecc -o stage2/test/macro2.s - -S
	$(CC) -o $@ stage2/test/macro2.s -xc test/common
	./stage2/test/macro2.exe

stage2/test/%.exe: stage2/ecc test/%.c
	mkdir -p stage2/test
	./stage2/ecc -o stage2/test/$*.s test/$*.c -S
	$(CC) -o $@ stage2/test/$*.s -xc test/common

test-stage2: $(TESTS:test/%=stage2/test/%)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done

# Stage 3

stage3/ecc: $(OBJS:%=stage3/%)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

stage3/%.s: stage2/ecc self.py %.c
	mkdir -p stage3
	python3 self.py ecc.h $*.c > stage3/$*.c
	./stage2/ecc stage3/$*.c -S -o stage3/$*.s

stage3/test/macro.exe: stage3/ecc test/macro.c
	mkdir -p stage3/test
	./stage3/ecc test/macro.c -S -o stage3/test/macro.s
	$(CC) -o $@ stage3/test/macro.s -xc test/common

stage3/test/macro2.exe: stage3/ecc test/macro.c
	mkdir -p stage3/test
	./stage3/ecc test/macro.c -E | ./stage3/ecc -o stage3/test/macro2.s - -S
	$(CC) -o $@ stage3/test/macro2.s -xc test/common
	./stage3/test/macro2.exe

stage3/test/%.exe: stage3/ecc test/%.c
	mkdir -p stage3/test
	./stage3/ecc -o stage3/test/$*.s test/$*.c -S
	$(CC) -o $@ stage3/test/$*.s -xc test/common

test-stage3: $(TESTS:test/%=stage3/test/%)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done

clean:
		rm -f ecc *.o
		rm -f test/*.s test/*.o test/*.exe test/_*
		rm -rf stage2
		rm -rf stage3

.PHONY: test clean