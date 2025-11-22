#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char *argv[]) { // dopisanie inputow maina
  pid_t child_pid = fork(); // dodalem tutaj zeby nie duplikowac
  switch( child_pid ) {
   case 0:
     sleep(3);
     printf("Potomek: %d kończy działanie\n",getpid());
     exit(EXIT_SUCCESS);
   case -1:
     perror("FORK");
     exit (EXIT_FAILURE);
   default:
    {
      
     if (argc > 1) sleep(atoi(argv[1]));

     /* Wyślij do potomka sygnał SIGINT i obsłuż błąd */
     if(kill(child_pid,SIGINT) == -1){
      perror("kill");
      exit(EXIT_FAILURE);}
  /* Czekaj na zakończenie potomka i pobierz status. 
     1) Jeśli potomek zakończył się normalnie (przez exit), wypisz informację wraz wartością statusu i jego PID. 
     2) Jeśli potomek zakończył się sygnałem, zwróć taką informację (wartość sygnału) */

      int status;
    pid_t term_pid = wait(&status);
    if (term_pid == -1) {
      perror("wait");
      exit(EXIT_FAILURE);}

    if(WIFEXITED(status) ){
      printf("Potomek %d zakończył się exitem. Status: %d\n", term_pid, WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
      printf("Potomk %d zakonczyl sie przez sygnal %d\n", term_pid, WTERMSIG(status));
  } 
	}
 }
  return 0;
}
