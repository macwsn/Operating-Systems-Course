CC = gcc
CFLAGS = -lrt

all: client server

server: server.c
	$(CC) server.c -o server $(CFLAGS) 

client: client.c
	$(CC) client.c -o client $(CFLAGS) -pthread

clean:
	rm -f client server

.PHONY: all clean
