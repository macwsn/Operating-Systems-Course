#define _GNU_SOURCE

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int pid = 0;
int mode = 0;
bool is_answer = false;

void handleSignal(int sig) {
    is_answer = true;
    printf("Received from catcher  \n");
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        return 1;}

    pid = atoi(argv[1]); // konwerwsja na inta
    mode = atoi(argv[2]);

    printf("Sender PID: %d, sending mode: %d to catcher PID: %d\n", getpid(), mode, pid);

    struct sigaction sa;
    sa.sa_handler = handleSignal; // ustawiamy funckje obslugi
    sigemptyset(&sa.sa_mask); // czyscimy maske
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL); // reset handlowania

    union sigval value;
    value.sival_int = mode; // ustawiamy value sygnalu na mode
    if (sigqueue(pid, SIGUSR1, value) == -1) { // blad z wyslaniem signalu
        return 1;
    }

    while (!is_answer) {
        pause();
    }

    return 0;
}
