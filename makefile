CC		= gcc
OBJS	= main.o calc.o stack.o io.o
STD		= c99
CFLAGS	= -c -g -Wall -pedantic -std=$(STD)
LDFLAGS	= -lgmp
PGM		= rpn

all:	$(PGM)

$(PGM):	$(OBJS)
		$(CC) -o $(PGM) $(OBJS) $(LDFLAGS)

%.o:	%.c
		$(CC) $(CFLAGS) $<

clean:
		rm -f $(PGM)
		rm -f $(OBJS)

# For development

check:
		cppcheck -q --std=$(STD) *.c *.h

valgrind: $(PGM)
		valgrind ./$(PGM)

test: $(PGM)
		bash ./tests/functional_tests.sh
