#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>


#define SERVER_QUEUE_NAME "/server_queue" // nazwa kolejki
#define MAX_MSG_SIZE 512 // max dlugosc

mqd_t server_queue; // deskryptor servera
mqd_t client_queue; // deskryptor klienta 
char client_queue_name[64];
int client_id;

void cleanup() {
    mq_close(server_queue); // zamkniecie deskryptorow
    mq_close(client_queue);
    mq_unlink(client_queue_name); // usuwa z systemu
    printf("\nClient exiting.\n");
    exit(0);
}

void *receiver(void *arg) {
    char buffer[MAX_MSG_SIZE];
    while (1) {
        ssize_t bytes = mq_receive(client_queue, buffer, MAX_MSG_SIZE, NULL); // otwieranie wiadomosci
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("%s\n", buffer);
        }
    }
    return NULL;
}

int main() {
    srand(time(NULL));
    snprintf(client_queue_name, sizeof(client_queue_name), "/client_queue_%d", getpid()); // unikalna nazwa dla kolejki danego klienta

    struct mq_attr attr = { .mq_maxmsg = 10, .mq_msgsize = MAX_MSG_SIZE }; // ustawianie atrybutow dla kolejki
    client_queue = mq_open(client_queue_name, O_CREAT | O_RDONLY, 0666, &attr); // tworzenie i otworzenie kolejki dla klienta -rw
    if (client_queue == -1) {exit(1); }

    server_queue = mq_open(SERVER_QUEUE_NAME, O_WRONLY); // otworzenie kolejki serwera
    if (server_queue == -1) {exit(1);}

    char init_msg[MAX_MSG_SIZE];
    sprintf(init_msg, "INIT:%s", client_queue_name); // tworzenie inita 
    mq_send(server_queue, init_msg, strlen(init_msg) + 1, 0); // wyslanie inita

    char buffer[MAX_MSG_SIZE];
    mq_receive(client_queue, buffer, MAX_MSG_SIZE, NULL); // pobieramy zwrotna z serwera
    sscanf(buffer, "ID:%d", &client_id); // wypsiujemy id
    printf("Connected as client %d\n", client_id);

    signal(SIGINT, cleanup); // setujemy cleanup na ctrl+c
    pthread_t receiver_thread;
    pthread_create(&receiver_thread, NULL, receiver, NULL); // tworzymy nowy wątek wykonujący receiver

    while (1) {
        char msg[MAX_MSG_SIZE - 10]; // obcinka zeby moizna bylo dodac id i znak konca
        if (fgets(msg, sizeof(msg), stdin)) { // popbiernia linie
            msg[strcspn(msg, "\n")] = '\0'; // zmiania n na 0
            char send_buf[MAX_MSG_SIZE];
            sprintf(send_buf, "%d:%s", client_id, msg); // waidomosc z id
            mq_send(server_queue, send_buf, strlen(send_buf) + 1, 0); // wyslanie do kolejki
        }
    }

    cleanup();
    return 0;
}