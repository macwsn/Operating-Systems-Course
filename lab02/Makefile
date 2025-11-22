
.PHONY: all c_shared c_shared_dlopen c_static clean

all:
	make c_shared
	make c_shared_dlopen
	make c_static

c_shared: c_shared.c
	mkdir -p build
	make collatzsh -C collatz_library
	gcc -Wall -L ./collatz_library/build $< -lcollatzsh -Wl,-rpath=./collatz_library/build -o build/c_shared

c_shared_dlopen: c_shared_dlopen.c
	mkdir -p build
	make collatzsh -C collatz_library
	gcc -Wall -o build/c_shared_dlopen c_shared_dlopen.c -ldl

c_static: c_static.c 
	mkdir -p build
	make -C collatz_library collatzst
	gcc -Wall -o build/c_static $< collatz_library/build/libcollatzst.a

test_static:
	./build/c_shared

test_shared_so:
	./build/c_shared_dlopen

test_shared_dlopen:
	./build/c_static


clean:
	rm -rf build
	make -C collatz_library clean