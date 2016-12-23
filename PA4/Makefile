EXE=webproxy
CC=gcc
CFLAGS=-O3 -Wall -g -Wextra
CLEAN=rm -f $(EXE) *.o *.a

all: $(EXE)

webproxy:webproxy.c
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

clean:
	$(CLEAN)
