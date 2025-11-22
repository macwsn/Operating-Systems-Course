#include <stdio.h>
#include <dlfcn.h>

int (*collatz_conjecture)(int);
int (*test_collatz_convergence)(int, int);

int main() {
    void* handle = dlopen("collatz_library/build/libcollatzsh.so", RTLD_LAZY);
    if(!handle){/*error*/return 1;}

    collatz_conjecture = dlsym(handle, "collatz_conjecture");
    test_collatz_convergence = dlsym(handle, "test_collatz_convergence");

    printf("%d\n", collatz_conjecture(3));
    printf("%d\n", test_collatz_convergence(33, 333));

    dlclose(handle);
}