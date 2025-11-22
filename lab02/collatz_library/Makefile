
.PHONY: all collatzst collatzsh clean

all:
	make collatzst
	make collatzsh

collatzst: collatz.c
	mkdir -p build
	gcc -Wall -o build/collatzst.o -c collatz.c
	ar rcs build/libcollatzst.a build/collatzst.o

collatzsh: collatz.c
	mkdir -p build
	gcc -fPIC -o build/collatzsh.o -c collatz.c
	gcc -shared -o build/libcollatzsh.so build/collatzsh.o

clean:
	rm -rf build