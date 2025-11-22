#define _GNU_SOURCE // bez tego nie dziala signal.h

// czasami po wykonaniu sygnalu 2 catcher blokuje sie na sekunde wiec trzeba 2 razy wyslac nastepny sygnal zeby zadzialal

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int pid = 0;
int command = 0;
int request_count = 0;
int ignore_ctrl_c = 0;
int count = 0;

void handleSignal(int sig, siginfo_t *info, void *context) {
    if (sig == SIGUSR1) {
        request_count++;

        if (info->si_value.sival_int != 0) {
            printf("Signal %d received from process: %dwith value: %d  \n", sig, info->si_pid, info->si_value.sival_int);
            command = info->si_value.sival_int;
        } else {
            printf("Signal %d received from process: %d \n", sig, info->si_pid);
        }

        pid = info->si_pid;}}

void handleCtrlC(int sig) {
    if (ignore_ctrl_c == 1) {
        printf("\nWciśnięto CTRL + C \n"); }}

int main() {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = handleSignal;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGUSR1, &action, NULL) == -1) { return 1; }

    struct sigaction ctrlc_action;
    memset(&ctrlc_action, 0, sizeof(ctrlc_action));
    ctrlc_action.sa_handler = handleCtrlC;
    sigemptyset(&ctrlc_action.sa_mask);

    printf("Catcher PID: %d\n", getpid());
    while (1) {
        pause();

        if (pid != 0) {
            switch (command) {
                case 1:
                    printf("Otrzymanych żądań zmiany trybu: %d\n", request_count);
                    break;
                case 2:
                    while (command == 2) {
                        printf("%d\n", count++);
                        sleep(1); 
                    }
                    break;
                case 3:
                    printf("Ignorowanie Ctrl + C \n");
                    signal(SIGINT, SIG_IGN);
                    break;
                case 4:
                    printf("Przechwytywanie Ctrl + C\n");
                    signal(SIGINT, handleCtrlC);
                    ignore_ctrl_c = 1;
                    break;
                case 5:
                    printf("Zamykanie catchera \n");
                    return 0;
            }

            command = 0;
            kill(pid, SIGUSR1);
            pid = 0;
        }}
    return 0;}