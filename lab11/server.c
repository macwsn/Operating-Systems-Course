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
    int sockfd;
    char name[NAME_LEN];
    int active;
    time_t last_alive;
} client_t;

client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_sockfd;

void broadcast_message(const char *message, int sender_idx) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && i != sender_idx) {
            if (write(clients[i].sockfd, message, strlen(message)) < 0) {} }}
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int client_idx) {
    pthread_mutex_lock(&clients_mutex);
    if (clients[client_idx].active) {
        printf("INFO: Klient %s rozłączony.\n", clients[client_idx].name);
        close(clients[client_idx].sockfd);
        clients[client_idx].active = 0;
        memset(clients[client_idx].name, 0, NAME_LEN); }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    int client_idx = *(int*)arg;
    free(arg); 

    char buffer[BUFFER_SIZE];
    char name_buffer[NAME_LEN];

    if (read(clients[client_idx].sockfd, name_buffer, NAME_LEN - 1) <= 0) {
        printf("INFO: Nie udało się odczytać nazwy od klienta, zamykanie połączenia.\n");
        remove_client(client_idx);
        return NULL;}
    name_buffer[strcspn(name_buffer, "\r\n")] = 0; 
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].name, name_buffer) == 0) {
            char *errmsg = "ERROR: Nazwa jest już zajęta.\n";
            write(clients[client_idx].sockfd, errmsg, strlen(errmsg));
            close(clients[client_idx].sockfd);
            clients[client_idx].active = 0;
            pthread_mutex_unlock(&clients_mutex);
            return NULL;
        }}
    strncpy(clients[client_idx].name, name_buffer, NAME_LEN - 1);
    clients[client_idx].last_alive = time(NULL);
    pthread_mutex_unlock(&clients_mutex);

    sprintf(buffer, "OK: Witaj %s! Jesteś połączony.\n", clients[client_idx].name);
    write(clients[client_idx].sockfd, buffer, strlen(buffer));

    sprintf(buffer, "[SERVER] Dołączył użytkownik: %s\n", clients[client_idx].name);
    printf("INFO: Klient %s dołączył.\n", clients[client_idx].name);
    broadcast_message(buffer, client_idx);
    while (1) {
        int read_size = read(clients[client_idx].sockfd, buffer, BUFFER_SIZE - 1);
        if (read_size <= 0) {
            break; }
        buffer[read_size] = 0;
        buffer[strcspn(buffer, "\r\n")] = 0;
        pthread_mutex_lock(&clients_mutex);
        clients[client_idx].last_alive = time(NULL);
        pthread_mutex_unlock(&clients_mutex);
        
        if (strncmp(buffer, "LIST", 4) == 0) {
            char list_msg[BUFFER_SIZE] = "[SERVER] Aktywni użytkownicy: ";
            pthread_mutex_lock(&clients_mutex);
            int first = 1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active) {
                    if (!first) {
                       strcat(list_msg, ", ");
                    }
                    strcat(list_msg, clients[i].name);
                    first = 0;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            strcat(list_msg, "\n");
            write(clients[client_idx].sockfd, list_msg, strlen(list_msg));

        } else if (strncmp(buffer, "2ALL ", 5) == 0) {
            char msg_to_send[BUFFER_SIZE];
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char time_str[30];
            strftime(time_str, sizeof(time_str)-1, "%Y-%m-%d %H:%M:%S", t);

            sprintf(msg_to_send, "[%s] [%s]: %s\n", time_str, clients[client_idx].name, buffer + 5);
            broadcast_message(msg_to_send, client_idx);

        } else if (strncmp(buffer, "2ONE ", 5) == 0) {
            char *p = buffer + 5; 
            char *space = strchr(p, ' '); 

            if (space != NULL) {
                *space = '\0'; 
                char *recipient_name = p;
                char *msg = space + 1;
                while (*msg == ' ') {msg++;}

                if (strlen(recipient_name) > 0 && strlen(msg) > 0) {
                    int found = 0;
                    char msg_to_send[BUFFER_SIZE];
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    char time_str[30];
                    strftime(time_str, sizeof(time_str) - 1, "%Y-%m-%d %H:%M:%S", t);
                    sprintf(msg_to_send, "(prywatnie) [%s] [%s]: %s\n", time_str, clients[client_idx].name, msg);
                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].active && strcmp(clients[i].name, recipient_name) == 0) {
                            if (write(clients[i].sockfd, msg_to_send, strlen(msg_to_send)) < 0) {
                                perror("Błąd write w 2ONE");
                            }
                            found = 1;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    if (!found) {
                        char *errmsg = "[SERVER] Błąd: Nie znaleziono użytkownika.\n";
                        write(clients[client_idx].sockfd, errmsg, strlen(errmsg));
                    }
                } else {
                    char *errmsg = "[SERVER] Błąd: Zły format. Użyj: 2ONE <nazwa> <wiadomosc>\n";
                    write(clients[client_idx].sockfd, errmsg, strlen(errmsg));}
            } else {
                char *errmsg = "[SERVER] Błąd: Zły format. Użyj: 2ONE <nazwa> <wiadomosc>\n";
                write(clients[client_idx].sockfd, errmsg, strlen(errmsg));}

        } else if (strncmp(buffer, "STOP", 4) == 0) {break;
        } else if (strncmp(buffer, "ALIVE_ACK", 9) == 0) {
        } else {
            char *errmsg = "[SERVER] Błąd: Nieznana komenda.\n";
            write(clients[client_idx].sockfd, errmsg, strlen(errmsg));
        }
    }
    char left_msg[BUFFER_SIZE];
    sprintf(left_msg, "[SERVER] Użytkownik %s opuścił czat.\n", clients[client_idx].name);
    remove_client(client_idx);
    broadcast_message(left_msg, -1);

    return NULL;}
void *alive_checker(void *arg) {
    while (1) {
        sleep(ALIVE_CHECK_INTERVAL);
        pthread_mutex_lock(&clients_mutex);
        time_t current_time = time(NULL);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                if (current_time - clients[i].last_alive > CLIENT_TIMEOUT) {
                    printf("INFO: Klient %s przekroczył timeout. Rozłączanie.\n", clients[i].name);
                    char msg[BUFFER_SIZE];
                    sprintf(msg, "[SERVER] Użytkownik %s został usunięty (timeout).\n", clients[i].name);
                    close(clients[i].sockfd);
                    clients[i].active = 0;
                    char client_name_copy[NAME_LEN];
                    strcpy(client_name_copy, clients[i].name);
                    memset(clients[i].name, 0, NAME_LEN);

                    pthread_mutex_unlock(&clients_mutex);
                    broadcast_message(msg, -1);
                    pthread_mutex_lock(&clients_mutex);
                } else {
                    if (write(clients[i].sockfd, "ALIVE\n", 6) < 0) {}
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    return NULL;}

void shutdown_server(int sig) {
    printf("\nINFO: Zamykanie serwera...\n");
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            write(clients[i].sockfd, "[SERVER] Serwer jest zamykany.\n", 32);
            close(clients[i].sockfd);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    close(server_sockfd);
    exit(0);}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Użycie: %s <adres_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);}

    char *ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    for (int i = 0; i < MAX_CLIENTS; i++) { clients[i].active = 0;}

    signal(SIGINT, shutdown_server);
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("Błąd socket");
        exit(EXIT_FAILURE);}
    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Błąd setsockopt");}

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Błąd bind");
        exit(EXIT_FAILURE);}

    if (listen(server_sockfd, 5) < 0) {
        perror("Błąd listen");
        exit(EXIT_FAILURE);}
    printf("INFO: Serwer nasłuchuje na %s:%d\n", ip, port);
    pthread_t alive_thread;
    if (pthread_create(&alive_thread, NULL, alive_checker, NULL) != 0) {
        perror("Błąd pthread_create dla alive_checker");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd < 0) {
            perror("Błąd accept");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        int client_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                client_idx = i;
                break;
            }
        }
        if (client_idx == -1) {
            printf("INFO: Serwer pełny, odrzucono połączenie.\n");
            char *errmsg = "ERROR: Serwer jest pełny.\n";
            write(client_sockfd, errmsg, strlen(errmsg));
            close(client_sockfd);
            pthread_mutex_unlock(&clients_mutex);
            continue;}
        clients[client_idx].sockfd = client_sockfd;
        clients[client_idx].active = 1;
        pthread_mutex_unlock(&clients_mutex);
        pthread_t tid;
        int *p_client_idx = malloc(sizeof(int));
        if (p_client_idx == NULL) {
            perror("Błąd malloc");
            close(client_sockfd);
            clients[client_idx].active = 0;
            continue;
        }
        *p_client_idx = client_idx;

        if (pthread_create(&tid, NULL, handle_client, (void*)p_client_idx) != 0) {
            perror("Błąd pthread_create dla handle_client");
            clients[client_idx].active = 0;
            free(p_client_idx);
        }
    }

    close(server_sockfd);
    return 0;}