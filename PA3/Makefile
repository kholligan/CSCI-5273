EXE=dfs dfc
CC=gcc
CFLAGS=-O3 -Wall -g -Wextra
LFLAGS=-lssl -lcrypto 
# -I/usr/local/opt/openssl/include -l/usr/local/opt/openssl/lib
CLEAN=rm -f $(EXE) *.o *.a

all: $(EXE)

dfc:dfsclient.c
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

dfs:dfsserver.c
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

clean:
	$(CLEAN)
