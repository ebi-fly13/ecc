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
		for i in $^; do echo $$i; ./$$i || exit 1; echo; done

# Stage 2

stage2/ecc: $(OBJS:%=stage2/%)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

stage2/%.s: ecc self.py %.c
	mkdir -p stage2
	python3 self.py ecc.h $*.c > stage2/$*.c
	./ecc stage2/$*.c > stage2/$*.s

stage2/test/%.exe: stage2/ecc test/%.c
	mkdir -p stage2/test
	$(CC) -o- -E -P -C test/$*.c > stage2/test/_.c
	./stage2/ecc stage2/test/_.c > stage2/test/$*.s
	$(CC) -o $@ stage2/test/$*.s -xc test/common
	rm stage2/test/_.c

test-stage2: $(TESTS:test/%=stage2/test/%)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done

# Stage 3

stage3/ecc: $(OBJS:%=stage3/%)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

stage3/%.s: stage2/ecc self.py %.c
	mkdir -p stage3
	python3 self.py ecc.h $*.c > stage3/$*.c
	./stage2/ecc stage3/$*.c > stage3/$*.s

stage3/test/%.exe: stage3/ecc test/%.c
	mkdir -p stage3/test
	$(CC) -o- -E -P -C test/$*.c > stage3/test/_.c
	./stage3/ecc stage3/test/_.c > stage3/test/$*.s
	$(CC) -o $@ stage3/test/$*.s -xc test/common
	rm stage3/test/_.c

test-stage3: $(TESTS:test/%=stage3/test/%)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done

clean:
		rm -f ecc *.o
		rm -f test/*.s test/*.o test/*.exe test/_*
		rm -rf stage2
		rm -rf stage3

.PHONY: test clean