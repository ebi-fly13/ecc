CFLAGS=-std=c11 -g -static
SRCS= main.c tokenize.c parse.c type.c codegen.c file.c
OBJS=$(SRCS:.c=.o)

ecc: $(OBJS)
		$(CC) -o ecc $(OBJS) $(LDFLAGS)

$(OBJS): ecc.h

test: ecc
		./test.sh

clean:
		rm -f ecc *.o *~ tmp*

.PHONY: test clean