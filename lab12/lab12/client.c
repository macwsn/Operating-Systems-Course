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

volatile sig_atomic_t stop_flag = 0;
int sockfd;
struct sockaddr_in server_addr;
char client_name[NAME_LEN];

void *receive_handler(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    while (!stop_flag) {
        socklen_t len = sizeof(server_addr);
        int read_size = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
        if (read_size > 0) {
            buffer[read_size] = '\0';
            
            if (strncmp(buffer, "ALIVE", 5) == 0) {
                char ack_msg[BUFFER_SIZE];
                sprintf(ack_msg, "%s ALIVE_ACK\n", client_name);
                sendto(sockfd, ack_msg, strlen(ack_msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
            } else {
                printf("\r%s", buffer);
                printf("Ty> ");
                fflush(stdout);
            }
        } else if (read_size < 0 && !stop_flag) {
            perror("Błąd recvfrom");
            stop_flag = 1;
        }
    }
    return NULL;
}

void *send_handler(void *arg) {
    (void)arg;
    char user_input[BUFFER_SIZE];
    char message_to_send[BUFFER_SIZE];
    sprintf(message_to_send, "%s LIST\n", client_name);
    sendto(sockfd, message_to_send, strlen(message_to_send), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    while (!stop_flag) {
        printf("Ty> ");
        fflush(stdout);
        if (fgets(user_input, BUFFER_SIZE, stdin) == NULL) {
            stop_flag = 1;
            break;}

        user_input[strcspn(user_input, "\n")] = 0;
        if (strlen(user_input) == 0) continue;

        sprintf(message_to_send, "%s %s\n", client_name, user_input);
        
        if (sendto(sockfd, message_to_send, strlen(message_to_send), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Błąd sendto");
            stop_flag = 1;}
        
        if (strcmp(user_input, "STOP") == 0) {
            stop_flag = 1;}
    }
    return NULL;
}

void handle_sigint(int sig) {
    (void)sig;
    if (stop_flag == 0) {
        stop_flag = 1;
        printf("\nINFO: Wykryto Ctrl+C. Wysyłanie STOP...\n");
        char stop_msg[BUFFER_SIZE];
        sprintf(stop_msg, "%s STOP\n", client_name);
        sendto(sockfd, stop_msg, strlen(stop_msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Użycie: %s <nazwa> <ip_serwera> <port_serwera>\n", argv[0]);
        exit(EXIT_FAILURE);}

    strncpy(client_name, argv[1], NAME_LEN - 1);
    client_name[NAME_LEN - 1] = '\0';
    char *ip = argv[2];
    int port = atoi(argv[3]);

    if (strlen(client_name) >= NAME_LEN) {
        fprintf(stderr, "Błąd: Nazwa nie może być dłuższa niż %d znaków.\n", NAME_LEN - 1);
        exit(EXIT_FAILURE); }

    signal(SIGINT, handle_sigint);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Błąd socket");
        exit(EXIT_FAILURE);}

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_aton(ip, &server_addr.sin_addr) == 0) {
        fprintf(stderr, "Błąd: Nieprawidłowy adres IP\n");
        exit(EXIT_FAILURE); }

    pthread_t send_thread, recv_thread;
    
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0) {
        perror("Błąd pthread_create dla receive_handler");
        return 1;}

    if (pthread_create(&send_thread, NULL, send_handler, NULL) != 0) {
        perror("Błąd pthread_create dla send_handler");
        return 1;}
    
    pthread_join(send_thread, NULL);
    pthread_cancel(recv_thread); 
    pthread_join(recv_thread, NULL);

    close(sockfd);
    printf("INFO: Połączenie zakończone.\n");
    return 0;
}