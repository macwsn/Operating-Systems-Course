#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#define SERVER_QUEUE_NAME "/server_queue" // nazwa kolejki 
#define MAX_CLIENTS 10 // maksymalna liczba clientow
#define MAX_MSG_SIZE 512 // maksymalna dlugosc wiadomosci

typedef struct { // dane klienta na serwerze 
    int id; // unikalne id
    char queue_name[64]; // nazwa kolejki w ktorej uczestniczy
    mqd_t queue; // deskryptor
} Client;

Client clients[MAX_CLIENTS]; // talibca przechowujaca dane aktualncyh kilentow
int client_count = 0;
mqd_t server_queue; // deskryptor kolejki serwera

void cleanup() {
    mq_close(server_queue); // zamkniecie deskryptora 
    mq_unlink(SERVER_QUEUE_NAME); // zakmniecie kolejki
    for (int i = 0; i < client_count; i++) { mq_close(clients[i].queue);} // zamkniecie deskryptora kazdego klienta
    printf("\nServer OFF.\n");
    exit(0);
}

void handle_init(char *msg_text) {
    if (client_count >= MAX_CLIENTS) return; // max max_clients

    char client_queue_name[64];
    sscanf(msg_text, "INIT:%s", client_queue_name); // wyciaga nazwa klienta

    mqd_t client_q = mq_open(client_queue_name, O_WRONLY); // otwiera nowa kolejke dla klienta
    if (client_q == -1) { return; }

    Client new_client; // tworzenie nowego klienta
    new_client.id = client_count;
    strcpy(new_client.queue_name, client_queue_name);
    new_client.queue = client_q;
    clients[client_count++] = new_client;

    char reply[64];
    sprintf(reply, "ID:%d", new_client.id); // wypisuje klientowi jesgo id
    mq_send(client_q, reply, strlen(reply) + 1, 0); // wysyla odpowiedz do kolejki kolienta
}

void broadcast_message(int sender_id, const char *msg) {
    char full_msg[MAX_MSG_SIZE];
    snprintf(full_msg, MAX_MSG_SIZE, "Client %d: %s", sender_id, msg); // dodajemy przedrostek

    for (int i = 0; i < client_count; i++) {
        if (clients[i].id != sender_id) { // jesli nie nadawaca, wysylka wiadomosc do kolejki
            mq_send(clients[i].queue, full_msg, strlen(full_msg) + 1, 0);
        }
    }
}

int main() {
    struct mq_attr attr = { .mq_maxmsg = 10, .mq_msgsize = MAX_MSG_SIZE }; // atrybuty dla kolejki
    server_queue = mq_open(SERVER_QUEUE_NAME, O_CREAT | O_RDONLY, 0666, &attr); // tworzy i otwiera kolejke rw-
    if (server_queue == -1) { exit(1);}

    signal(SIGINT, cleanup); // czysci stare
    printf("Server ON\n");

    char buffer[MAX_MSG_SIZE];
    while (1) {
        ssize_t bytes = mq_receive(server_queue, buffer, MAX_MSG_SIZE, NULL); // otwiera wiadomosc 
        if (bytes > 0) {
            buffer[bytes] = '\0';
            if (strncmp(buffer, "INIT:", 5) == 0) { // jesli init to go obcina
                handle_init(buffer);
            } else {
                int client_id;
                char msg[MAX_MSG_SIZE];
                sscanf(buffer, "%d:%[^\n]", &client_id, msg); // wyodrebniamy id i reszte
                broadcast_message(client_id, msg);
            }
        }
    }

    cleanup();
    return 0;
}
