#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 2048
#define NAME_LEN 20

volatile sig_atomic_t flag = 0;
int sockfd;

void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int read_size = read(sockfd, buffer, BUFFER_SIZE - 1);
        if (read_size > 0) {
            buffer[read_size] = '\0';
            
            // ALIVE
            if (strncmp(buffer, "ALIVE", 5) == 0) {
                write(sockfd, "ALIVE_ACK\n", 10);
            } else {
                printf("\r%s", buffer); 
                printf("Ty> "); 
                fflush(stdout);
            }
        } else if (read_size == 0) {
            printf("\nINFO: Serwer zamknął połączenie.\n");
            flag = 1;
            break;
        } else {
            break;
        }
    }
    return NULL;
}

void *send_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + NAME_LEN];

    while (!flag) {
        printf("Ty> ");
        fflush(stdout);
        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;

            if (strlen(buffer) == 0) continue;

            if (strcmp(buffer, "STOP") == 0) {
                flag = 1;
                write(sockfd, "STOP\n", 5);
                break;}
            sprintf(message, "%s\n", buffer);
            if (write(sockfd, message, strlen(message)) < 0) {
                perror("Błąd write");
                flag = 1;
                break;}
        } else {
            flag = 1;
            write(sockfd, "STOP\n", 5);
            break;
        }}
    return NULL;}

void handle_sigint(int sig) {
    flag = 1;
    printf("\nINFO: Wykryto Ctrl+C. Zamykanie...\n");
    if (sockfd > 0) { write(sockfd, "STOP\n", 5);}}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Użycie: %s <nazwa> <ip_serwera> <port_serwera>\n", argv[0]);
        exit(EXIT_FAILURE);}

    char *name = argv[1];
    char *ip = argv[2];
    int port = atoi(argv[3]);

    if (strlen(name) >= NAME_LEN) {
        fprintf(stderr, "Błąd: Nazwa nie może być dłuższa niż %d znaków.\n", NAME_LEN - 1);
        exit(EXIT_FAILURE);}

    signal(SIGINT, handle_sigint);

    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Błąd socket");
        exit(EXIT_FAILURE);}

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Błąd connect");
        exit(EXIT_FAILURE);}

    char name_with_newline[NAME_LEN + 1];
    snprintf(name_with_newline, sizeof(name_with_newline), "%s\n", name);
    write(sockfd, name_with_newline, strlen(name_with_newline));
    char server_response[BUFFER_SIZE];
    int read_size = read(sockfd, server_response, BUFFER_SIZE - 1);
    if (read_size <= 0) {
        printf("Bład:Nie otrzymano odpowiedzi od serwera.\n");
        close(sockfd);
        return 1;}
    server_response[read_size] = '\0';

    if (strncmp(server_response, "ERROR", 5) == 0) {
        printf("Błąd od serwera: %s", server_response);
        close(sockfd);
        return 1;}
    printf("%s", server_response);
    pthread_t send_thread, recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0) {
        perror("Błąd pthread_create dla receive_handler");
        return 1;}

    if (pthread_create(&send_thread, NULL, send_handler, NULL) != 0) {
        perror("Błąd pthread_create dla send_handler");
        return 1;}

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);
    close(sockfd);
    printf("INFO: Połączenie zakończone.\n");
    return 0;}