CC		= gcc
OBJS	= main.o calc.o stack.o io.o
STD		= c99
CFLAGS	= -c -g -Wall -pedantic -std=$(STD) -MMD -MP
LDFLAGS	= -lgmp
PGM		= rpn

all:	$(PGM)

$(PGM):	$(OBJS)
		$(CC) -o $(PGM) $(OBJS) $(LDFLAGS)

%.o:	%.c
		$(CC) $(CFLAGS) $<

-include $(OBJS:.o=.d)

clean:
		rm -f $(PGM)
		rm -f $(OBJS) $(OBJS:.o=.d)

# For development

check:
		cppcheck -q --error-exitcode=1 --std=$(STD) *.c *.h

valgrind: $(PGM)
		valgrind ./$(PGM)

test: $(PGM)
		bash ./tests/functional_tests.sh
