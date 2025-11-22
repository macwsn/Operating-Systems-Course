CC = gcc


all: integrate

integrate: integrate.c
	$(CC) integrate.c -o integrate -lpthread -lm

clean:
	rm -f integrate

.PHONY: all clean