CC		= gcc
OBJS	= main.o calc.o stack.o utils.o
STD		= c99
CFLAGS	= -c -g -Wall -pedantic -std=$(STD)
LDFLAGS	= -lm
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
