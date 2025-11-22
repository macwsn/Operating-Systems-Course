#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int global = 0; 

int main(int argc, char *argv[]) {
    int local = 0; 

    printf("NazWA: %s\n", argv[0]);

    if (argc != 2) {return EXIT_FAILURE; }

    pid_t pid = fork();

    if (pid == -1) { return EXIT_FAILURE;}
    else if (pid == 0) {

        printf("child process\n");
        
        global++;
        local++;
        
        printf("child = %d, parent = %d\n", getpid(), getppid());
        printf("local = %d,global = %d\n", local, global);
        
        execl("/bin/ls", "ls", argv[1], NULL);
        
        perror("execl failed");
        return EXIT_FAILURE;
    } else {
        printf("parent process\n");
        
        int status;
        waitpid(pid, &status, 0); // czekamy na zakonczenie procesu o danym PID
        
        printf("parent pid = %d, child pid = %d\n", getpid(), pid);
        
        if (WIFEXITED(status)) { // czy noirmalnie
            printf("Child exit code: %d\n", WEXITSTATUS(status));
        } else {
            printf("Not normally\n");
        }
        
        printf("local = %d, global = %d\n", local, global);
        
        return EXIT_SUCCESS;
    }
}