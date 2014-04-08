all: cser

CFLAGS=-std=c99 -O2 -pipe -g -Wall -Wextra -MMD -D_GNU_SOURCE

SRCS=\
	cser.c \
	frontend.c \
	backend_raw.c \

AUTO_SRCS=\
	c11_lexer.c \
	c11_parser.c \

OBJS=$(SRCS:.c=.o) $(AUTO_SRCS:.c=.o)
DEPS=$(SRCS:.c=.d)

c11_lexer.c: c11.l c11_parser.h
	flex $<

c11_parser.h: c11_parser.c
$(SRCS): c11_parser.h

c11_parser.c: c11.y
	bison -d $< -o $@

cser: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

.PHONY: clean
clean:
	rm -f cser $(AUTO_SRCS) $(AUTO_SRCS:.c=.h) *.o *.d


out.c:
	./cser

test: out.c test.c
	$(CC) $(CFLAGS) -O0 $^ -o $@

sinclude $(DEPS)
