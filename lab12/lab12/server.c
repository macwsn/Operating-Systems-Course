#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2048
#define NAME_LEN 20
#define ALIVE_CHECK_INTERVAL 30 
#define CLIENT_TIMEOUT 65       

typedef struct {
    struct sockaddr_in addr;
    char name[NAME_LEN];
    int active; 
    time_t last_alive;
} client_t;

client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_sockfd;

void broadcast_message(const char *message, const char *sender_name) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].name, sender_name) != 0) {
            sendto(server_sockfd, message, strlen(message), 0,
                   (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *alive_checker(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        sleep(ALIVE_CHECK_INTERVAL);
        pthread_mutex_lock(&clients_mutex);
        time_t current_time = time(NULL);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                if (current_time - clients[i].last_alive > CLIENT_TIMEOUT) {
                    printf("INFO: Klient %s przekroczył timeout. Usuwanie.\n", clients[i].name);
                    sprintf(buffer, "[SERVER] Użytkownik %s został usunięty (timeout).\n", clients[i].name);
                    
                    char name_copy[NAME_LEN];
                    strcpy(name_copy, clients[i].name);
                    clients[i].active = 0;
                    
                    pthread_mutex_unlock(&clients_mutex);
                    broadcast_message(buffer, name_copy); 
                    pthread_mutex_lock(&clients_mutex);
                } else {
                    sendto(server_sockfd, "ALIVE\n", 6, 0,
                           (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    return NULL;
}

void shutdown_server(int sig) {
    (void)sig;
    printf("\nINFO: Zamykanie serwera...\n");
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            sendto(server_sockfd, "[SERVER] Serwer jest zamykany.\n", 32, 0,
                   (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    close(server_sockfd);
    exit(0);
}

void process_message(char *buffer, struct sockaddr_in *client_addr) {
    char name[NAME_LEN];
    char command[BUFFER_SIZE];

    if (sscanf(buffer, "%19s %[^\n]", name, command) < 1) {
        return; }

    int client_idx = -1;
    int is_new_client = 0;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].name, name) == 0) {
            client_idx = i;
            break;}
    }
    if (client_idx == -1) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                client_idx = i;
                is_new_client = 1;
                strcpy(clients[i].name, name);
                clients[i].active = 1;
                break;}
        }
        if (client_idx == -1) { 
            char *errmsg = "ERROR: Serwer jest pełny.\n";
            sendto(server_sockfd, errmsg, strlen(errmsg), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }

    clients[client_idx].addr = *client_addr;
    clients[client_idx].last_alive = time(NULL);
    
    if (is_new_client) {
        printf("INFO: Nowy klient '%s' zarejestrowany. [%s:%d]\n", name, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
        char welcome_msg[BUFFER_SIZE];
        sprintf(welcome_msg, "OK: Witaj %s! Zostałeś zarejestrowany.\n", name);
        sendto(server_sockfd, welcome_msg, strlen(welcome_msg), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));

        sprintf(buffer, "[SERVER] Dołączył użytkownik: %s\n", name);
        pthread_mutex_unlock(&clients_mutex);
        broadcast_message(buffer, name);
        pthread_mutex_lock(&clients_mutex);
    }
    pthread_mutex_unlock(&clients_mutex);

    if (strcmp(command, "LIST") == 0) {
        char list_msg[BUFFER_SIZE] = "[SERVER] Aktywni użytkownicy: ";
        pthread_mutex_lock(&clients_mutex);
        int first = 1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                if (!first) strcat(list_msg, ", ");
                strcat(list_msg, clients[i].name);
                first = 0;}
        }
        pthread_mutex_unlock(&clients_mutex);
        strcat(list_msg, "\n");
        sendto(server_sockfd, list_msg, strlen(list_msg), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));

    } else if (strncmp(command, "2ALL ", 5) == 0) {
        char msg_to_send[BUFFER_SIZE];
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char time_str[30];
        strftime(time_str, sizeof(time_str) - 1, "%Y-%m-%d %H:%M:%S", t);
        sprintf(msg_to_send, "[%s] [%s]: %s\n", time_str, name, command + 5);
        broadcast_message(msg_to_send, name);

    } else if (strncmp(command, "2ONE ", 5) == 0) {
        char recipient_name[NAME_LEN];
        char msg[BUFFER_SIZE];
        if (sscanf(command + 5, "%19s %[^\n]", recipient_name, msg) == 2) {
            int found = 0;
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active && strcmp(clients[i].name, recipient_name) == 0) {
                    char msg_to_send[BUFFER_SIZE];
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    char time_str[30];
                    strftime(time_str, sizeof(time_str) - 1, "%Y-%m-%d %H:%M:%S", t);
                    sprintf(msg_to_send, "(prywatnie) [%s] [%s]: %s\n", time_str, name, msg);
                    sendto(server_sockfd, msg_to_send, strlen(msg_to_send), 0, (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            if (!found) {
                char *errmsg = "[SERVER] Błąd: Nie znaleziono użytkownika.\n";
                sendto(server_sockfd, errmsg, strlen(errmsg), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
            }
        }
    } else if (strcmp(command, "STOP") == 0) {
        printf("INFO: Klient %s wysłał STOP.\n", name);
        char left_msg[BUFFER_SIZE];
        sprintf(left_msg, "[SERVER] Użytkownik %s opuścił czat.\n", name);
        pthread_mutex_lock(&clients_mutex);
        clients[client_idx].active = 0;
        pthread_mutex_unlock(&clients_mutex);
        broadcast_message(left_msg, name);
    } else if (strcmp(command, "ALIVE_ACK") == 0) {}
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Użycie: %s <adres_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].active = 0;

    signal(SIGINT, shutdown_server);

    server_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sockfd < 0) {
        perror("Błąd socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Błąd bind");
        exit(EXIT_FAILURE);
    }

    printf("INFO: Serwer UDP nasłuchuje na %s:%d\n", ip, port);

    pthread_t alive_thread;
    if (pthread_create(&alive_thread, NULL, alive_checker, NULL) != 0) {
        perror("Błąd pthread_create dla alive_checker");
        exit(EXIT_FAILURE);
    }

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        int read_size = recvfrom(server_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_len);
        if (read_size > 0) {
            buffer[read_size] = '\0';
            buffer[strcspn(buffer, "\r\n")] = 0;
            process_message(buffer, &client_addr);
        }
    }
    return 0;
}