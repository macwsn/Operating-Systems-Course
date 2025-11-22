#include <stdio.h>
#include "collatz_library/collatz.h"

int main() {
    printf("%d\n", collatz_conjecture(3));
    printf("%d\n", test_collatz_convergence(33, 333));
}