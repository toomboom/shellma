SRC = main.c strbuf.c debug.c lexer.c parser.c shell.c executor.c wrappers.c
OBJ = $(SRC:.c=.o)
CFLAGS = -ggdb -Wall -pedantic -DDEBUG

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

shellma: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

ifneq (clean, $(MAKECMDGOALS))
-include deps.mk
endif

deps.mk: $(SRC)
	$(CC) -MM $^ > deps.mk

clean:
	rm -f *.o shellma deps.mk
