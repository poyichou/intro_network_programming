.PHONY: all clean
CC=gcc
FLAG=-g -Wall
EXECFILE=claw_machine
all: $(EXECFILE)
clean:
	rm $(EXECFILE)
%: %.c
	$(CC) $(FLAG) -pthread -o $@ $^
