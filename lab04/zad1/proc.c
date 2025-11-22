#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

    int n = atoi(argv[1]); // na inta

    for (int i = 0; i < n; i++) {
        pid_t child_pid = fork();// zbieramy nowy proces
        if (child_pid == -1) {
            perror("fork");//zwraca blad
            return 1; }

        if (child_pid == 0) {//proces potomny
            printf("%d %d\n", getppid(), getpid());//get(parent)pid
            exit(0);} // konczy proces
    }

    for (int i = 0; i < n; i++) {
        wait(NULL);} // blokowanie rodzica dopoki sie nie zakoncza potomne

    printf("%s\n", argv[1]);
    return 0;
}