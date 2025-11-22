#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>


// none, proces sie konczy
// ignore, ignorowanie i invisible
// handler, handler i invisible
// mask , zablokowany i visible


void handle(int signum) {
    printf("Received SIGUSR1\n");
}

int main(int argc, char *argv[]) { // walidacja
    if (argc != 2) {
        fprintf(stderr, "Uzyj: %s <none|ignore|handler|mask>\n", argv[0]);
        return -1;
    }

    sigset_t sigset;

    if (strcmp(argv[1], "ignore") == 0) {
        signal(SIGUSR1, SIG_IGN); // ustaiwnie ignorowania
    } else if (strcmp(argv[1], "handler") == 0) {
        signal(SIGUSR1, handle); // ustawiamy handler 
    } else if (strcmp(argv[1], "mask") == 0) {
        sigemptyset(&sigset);  // zesatw sygnalow
        sigaddset(&sigset, SIGUSR1); // dodanie do seta naszego sygnalu
        sigprocmask(SIG_BLOCK, &sigset, NULL); // blokowanie sigseta
    } else if (strcmp(argv[1], "none") == 0) {
        signal(SIGUSR1, SIG_DFL); // deafulatowa akcja
        raise(SIGUSR1); // wyslanie do procesu
    } else {
        return EXIT_FAILURE; // inny sygnal -> zle
    }

    raise(SIGUSR1); // wysylanie sygnalu do procesu aby sprawdzic jak sie zachowa

    sigpending(&sigset);
    if (sigismember(&sigset, SIGUSR1)) { // sprawdzamy czy nasz signal jest oczekujacy, jesli tak to visible, jesli nie invisible
        printf("visible");
    } else {
        printf("invisible");
    }
    printf("\n");
    return 1;
}