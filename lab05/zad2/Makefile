CC = gcc
CFLAGS = -Wall -std=c11

all: catcher sender

catcher: catcher.c
	$(CC) $(CFLAGS) -o catcher catcher.c

sender: sender.c
	$(CC) $(CFLAGS) -o sender sender.c

clean:
	rm -f catcher sender

.PHONY: all clean