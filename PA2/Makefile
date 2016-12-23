EXE=webserver
CC=gcc
CFLAGS=-O3 -Wall
CLEAN=rm -f $(EXE) *.o *.a

all: $(EXE)

webserver:webserver.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	$(CLEAN)
