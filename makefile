CC		= gcc
OBJS	= main.o calc.o stack.o utils.o
CFLAGS	= -c -g -Wall -pedantic -std=c99
LDFLAGS	= -lm
PGM		= rpn

all:	$(PGM)

$(PGM):	$(OBJS)
		$(CC) -o $(PGM) $(OBJS) $(LDFLAGS) 

.c.o:
		$(CC) $(CFLAGS) $<

clean:
		rm -f $(PGM)
		rm -f $(OBJS)
