#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#define SHM_NAME "/print_queue_shm" // pamiec wspoldzielona
#define SEM_MUTEX "/print_queue_mutex" // semafor binarny chroniac kolejke
#define SEM_SLOTS "/print_queue_slots" // wolne miejsca w kolejce
#define SEM_ITEMS "/print_queue_items" // zadania w kolejce

#define N 5         // userzy
#define M 3         // drukareki
#define QUEUE_SIZE 10  // rozmiar kolejki

typedef struct {
    char text[10]; // dlugosc wydruku
} print_job;

typedef struct {
    print_job jobs[QUEUE_SIZE]; // tablica zadania
    int head; // skad pobiera drukarka
    int tail; // gdzie dodaje uzytkownik
    int count;  // aktualna liczba zadan
    } print_queue;

print_queue *queue; // wskaznik do kolejki
sem_t *mutex, *slots, *items; // wskaznik do semaforow

void cleanup() {
    // zamkniecie i usuniecie semaforow
    sem_close(mutex);
    sem_close(slots);
    sem_close(items);
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_SLOTS);
    sem_unlink(SEM_ITEMS);
    
    // usuniecie pamieci wspoldzielonej
    munmap(queue, sizeof(print_queue));
    shm_unlink(SHM_NAME);
}

void *user_function(void *arg) {
    int id = *((int*)arg); // ID użytkownika
    
    srand(time(NULL) + id); // generator liczb losowych 
    
    while (1) {
        print_job job;
        for (int i = 0; i < 10; i++) {
            job.text[i] = 'a' + rand() % 26;  // losowych 10 znakow
        }
        
        printf("Użytkownik %d: chce dodać zadanie wydruku: %.10s\n", id, job.text);
        
        // Czekanie, aż będzie wolne miejsce w kolejce
        sem_wait(slots); // jesli slots(wolne) = 0 to watek czeka
        sem_wait(mutex); // wejscie do kolejki( jesli mutex = 0 to czeka)
        
        // dodanie zadania do kolejki
        queue->jobs[queue->tail] = job;
        queue->tail = (queue->tail + 1) % QUEUE_SIZE; // przesuwany cyklicznie tail
        queue->count++;
        
        printf("Użytkownik %d: dodał zadanie wydruku: %.10s (kolejka: %d/%d)\n", 
               id, job.text, queue->count, QUEUE_SIZE);
        
        sem_post(mutex); // wyjscie z sekcji i mutex++
        sem_post(items);  // wieksza items
        
        
        int sleep_time = 1 + rand() % 5; // losowe odczekanie
        printf("Użytkownik %d: czeka %d sekund przed kolejnym zadaniem\n", id, sleep_time);
        sleep(sleep_time);
    }
    
    return NULL;
}

void *printer_function(void *arg) {
    int id = *((int*)arg);
    
    while (1) {
        // oczekiwanie na zadanie
        printf("Drukarka %d: czeka na zadanie...\n", id);
        sem_wait(items);
        sem_wait(mutex);
        
        //bierzemy zadanie z kolejki
        print_job job = queue->jobs[queue->head];
        queue->head = (queue->head + 1) % QUEUE_SIZE; // cyklicznie przestawianie head
        queue->count--;
        
        printf("Drukarka %d: pobrała zadanie: %.10s (kolejka: %d/%d)\n", 
               id, job.text, queue->count, QUEUE_SIZE);
        
        sem_post(mutex);
        sem_post(slots);  // zwoln miejsce
        
        // printowanie co sekunde
        printf("Drukarka %d: rozpoczyna wydruk: ", id);
        fflush(stdout); // wymusza buffor
        
        for (int i = 0; i < 10; i++) {
            putchar(job.text[i]);
            fflush(stdout);
            sleep(1);  // znak co 1s
        }
        
        printf(" [koniec wydruku]\n");
    }
    
    return NULL;
}

int main() {
    pthread_t users[N], printers[M]; // identyfikatory
    int user_ids[N], printer_ids[M]; // id watkow

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); // tworzy/otwiera segment
    if (shm_fd == -1) {
        exit(EXIT_FAILURE);}
    
    // rozmiar wspoldzielonej
    if (ftruncate(shm_fd, sizeof(print_queue)) == -1) {
        exit(EXIT_FAILURE);
    }
    
    // mapowanie segmentu do pamieci
    queue = mmap(NULL, sizeof(print_queue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (queue == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    
    // zamkniecie deskryptora pliku
    close(shm_fd);
    
    // ustawianie kolejki
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    
    // tworzenie i inicjalizacja semaforow
    mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0666, 1);  // semafor do ochrony dostępu bin
    slots = sem_open(SEM_SLOTS, O_CREAT | O_EXCL, 0666, QUEUE_SIZE);  // wolne miejsca liczacy
    items = sem_open(SEM_ITEMS, O_CREAT | O_EXCL, 0666, 0);  // Liczba zadan liczacy
    
    if (mutex == SEM_FAILED || slots == SEM_FAILED || items == SEM_FAILED) {
        perror("sem_open");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    printf("System wydruku uruchomiony (%d użytkowników, %d drukarek, kolejka: %d)\n", N, M, QUEUE_SIZE);
    
    // watki usersow
    for (int i = 0; i < N; i++) {
        user_ids[i] = i + 1;
        if (pthread_create(&users[i], NULL, user_function, &user_ids[i]) != 0) {
            perror("pthread_create (user)");
            cleanup();
            exit(EXIT_FAILURE);
        }
    }
    
    // watki drukarek
    for (int i = 0; i < M; i++) {
        printer_ids[i] = i + 1;
        if (pthread_create(&printers[i], NULL, printer_function, &printer_ids[i]) != 0) {
            perror("pthread_create (printer)");
            cleanup();
            exit(EXIT_FAILURE);
        }
    }
    
    // czekamy na zakonczenie(tu nie)
    for (int i = 0; i < N; i++) {
        pthread_join(users[i], NULL);
    }
    
    for (int i = 0; i < M; i++) {
        pthread_join(printers[i], NULL);
    }
    
    cleanup();
    return 0;
}