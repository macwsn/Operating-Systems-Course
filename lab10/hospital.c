#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

// stale
#define MAX_PATIENTS_IN_WAITING_ROOM 3
#define PATIENTS_PER_CONSULTATION 3
#define MEDICINE_PER_CONSULTATION 3
#define MAX_MEDICINE_CAPACITY 6
#define MAX_ID_BUFFER_SIZE 1024 // lista id pacjentow przy printowaniu wyjsica od lekarza

pthread_mutex_t hospital_mutex;

pthread_cond_t doctor_cond;                   // czekajacy lekarz
pthread_cond_t patient_can_enter_cond;        // czekajacy pacjenci
pthread_cond_t pharmacist_can_deliver_cond;   // czekajacy farmaceuta
pthread_cond_t consultation_slots_filled_cond; // lekarz czeka na 3 pacjentow
pthread_cond_t* patient_consultation_finished_cond; 
pthread_cond_t pharmacist_delivery_accepted_cond; // czy lekarz przyjal dostawe(dla farmaceutow)

int patients_in_waiting_room = 0;
int patient_ids_for_current_consultation[PATIENTS_PER_CONSULTATION];
int medicine_count = 0; // apteczka

int total_patients_arg = 0;
int patients_served_count = 0;

bool pharmacist_wants_to_deliver = false; 
int delivering_pharmacist_id = -1;      // ID farmaceuty


char* get_time_str() { // wypisywanie czasu
    static char buffer[30];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return buffer;
}

int random_sleep_time(int min_sec, int max_sec) { // losowanie czasu
    return (rand() % (max_sec - min_sec + 1) + min_sec);
}

// lekarz
void* doctor_thread_func(void* arg) {
    (void)arg;
    printf("[%s] - Lekarz: Rozpoczynam dyżur.\n", get_time_str());

    pthread_mutex_lock(&hospital_mutex);
    while (patients_served_count < total_patients_arg) {

        while (!((patients_in_waiting_room == PATIENTS_PER_CONSULTATION && medicine_count >= MEDICINE_PER_CONSULTATION) ||
                 (pharmacist_wants_to_deliver && medicine_count < MAX_MEDICINE_CAPACITY && medicine_count < MEDICINE_PER_CONSULTATION))) {
            printf("[%s] - Lekarz: Zasypiam. Pacjentów w poczekalni: %d, Leków: %d.\n", get_time_str(), patients_in_waiting_room, medicine_count);
            pthread_cond_wait(&doctor_cond, &hospital_mutex);
            printf("[%s] - Lekarz: Zostałem obudzony sygnałem.\n", get_time_str());
        }
        printf("[%s] - Lekarz: Budzę się.\n", get_time_str());

        // obsluga pacjentow
        if (patients_in_waiting_room == PATIENTS_PER_CONSULTATION && medicine_count >= MEDICINE_PER_CONSULTATION) {
            char patient_ids_str[MAX_ID_BUFFER_SIZE] = "";
            for (int i = 0; i < PATIENTS_PER_CONSULTATION; ++i) {
                char temp[10];
                sprintf(temp, "%d", patient_ids_for_current_consultation[i]);
                strcat(patient_ids_str, temp);
                if (i < PATIENTS_PER_CONSULTATION - 1) {
                    strcat(patient_ids_str, ", ");
                }
            }
            printf("[%s] - Lekarz: Konsultuję pacjentów %s.\n", get_time_str(), patient_ids_str);
            pthread_mutex_unlock(&hospital_mutex);
            sleep(random_sleep_time(2, 4)); 
            pthread_mutex_lock(&hospital_mutex);
            medicine_count -= MEDICINE_PER_CONSULTATION;
            patients_served_count += PATIENTS_PER_CONSULTATION;

            printf("[%s] - Lekarz: Zakończono konsultację. Leków: %d. Obsłużonych: %d/%d\n", get_time_str(), medicine_count, patients_served_count, total_patients_arg);
            for (int i = 0; i < PATIENTS_PER_CONSULTATION; ++i) {
                pthread_cond_signal(&patient_consultation_finished_cond[patient_ids_for_current_consultation[i]]);
            }
            memset(patient_ids_for_current_consultation, 0, sizeof(patient_ids_for_current_consultation));
            patients_in_waiting_room = 0;


            
            pthread_cond_broadcast(&patient_can_enter_cond);

            
            if (medicine_count < MEDICINE_PER_CONSULTATION) {
                pthread_cond_broadcast(&pharmacist_can_deliver_cond); 
            }

        }

        else if (pharmacist_wants_to_deliver && medicine_count < MAX_MEDICINE_CAPACITY) {
            printf("[%s] - Lekarz: Przyjmuję dostawę leków od Farmaceuty(%d).\n", get_time_str(), delivering_pharmacist_id);

            pthread_mutex_unlock(&hospital_mutex);
            sleep(random_sleep_time(1, 3));
            pthread_mutex_lock(&hospital_mutex);

            medicine_count = MAX_MEDICINE_CAPACITY;
            pharmacist_wants_to_deliver = false;
            delivering_pharmacist_id = -1;

            printf("[%s] - Lekarz: Dostawa zakończona. Leków: %d.\n", get_time_str(), medicine_count);
            pthread_cond_signal(&pharmacist_delivery_accepted_cond);
            pthread_cond_broadcast(&pharmacist_can_deliver_cond);
        }
    }
    pthread_mutex_unlock(&hospital_mutex);
    printf("[%s] - Lekarz: Kończę dyżur, wszyscy pacjenci obsłużeni.\n", get_time_str());
    return NULL;
}

// pacjenci
void* patient_thread_func(void* arg) {
    int patient_id = *(int*)arg;
    free(arg); 

    int arrival_time = random_sleep_time(2, 5);
    printf("[%s] - Pacjent(%d): Idę do szpitala, będę za %d s.\n", get_time_str(), patient_id, arrival_time);
    sleep(arrival_time);

    pthread_mutex_lock(&hospital_mutex);
    while (true) { 
        if (patients_in_waiting_room >= MAX_PATIENTS_IN_WAITING_ROOM) {
            int walk_time = random_sleep_time(1, 3);
            printf("[%s] - Pacjent(%d): Za dużo pacjentów (%d), wracam później za %d s.\n", get_time_str(), patient_id, patients_in_waiting_room, walk_time);
            pthread_cond_wait(&patient_can_enter_cond, &hospital_mutex);
            continue;
        } else {
            patient_ids_for_current_consultation[patients_in_waiting_room] = patient_id;
            patients_in_waiting_room++;
            printf("[%s] - Pacjent(%d): Wszedłem. Czeka %d pacjentów na lekarza.\n", get_time_str(), patient_id, patients_in_waiting_room);
            if (patients_in_waiting_room == PATIENTS_PER_CONSULTATION) {
                printf("[%s] - Pacjent(%d): Jestem trzeci, budzę lekarza.\n", get_time_str(), patient_id);
                pthread_cond_signal(&doctor_cond); 
            }
            pthread_cond_wait(&patient_consultation_finished_cond[patient_id], &hospital_mutex);
            printf("[%s] - Pacjent(%d): Kończę wizytę.\n", get_time_str(), patient_id);
            break; 
        }
    }
    pthread_mutex_unlock(&hospital_mutex);
    return NULL;
}

// farmaceuta
void* pharmacist_thread_func(void* arg) {
    int pharmacist_id = *(int*)arg;
    free(arg);

    int arrival_time = random_sleep_time(5, 15);
    printf("[%s] - Farmaceuta(%d): Idę do szpitala z dostawą, będę za %d s.\n", get_time_str(), pharmacist_id, arrival_time);
    sleep(arrival_time);
    pthread_mutex_lock(&hospital_mutex);
    while (medicine_count == MAX_MEDICINE_CAPACITY || pharmacist_wants_to_deliver) {
        if (medicine_count == MAX_MEDICINE_CAPACITY) {
            printf("[%s] - Farmaceuta(%d): Apteczka pełna (%d/%d). Czekam.\n", get_time_str(), pharmacist_id, medicine_count, MAX_MEDICINE_CAPACITY);
        } else { 
            printf("[%s] - Farmaceuta(%d): Inny farmaceuta (%d) jest już zgłoszony. Czekam na swoją kolej.\n", get_time_str(), pharmacist_id, delivering_pharmacist_id);
        }
        pthread_cond_wait(&pharmacist_can_deliver_cond, &hospital_mutex);
    }
    delivering_pharmacist_id = pharmacist_id;
    pharmacist_wants_to_deliver = true;
    printf("[%s] - Farmaceuta(%d): Chcę dostarczyć leki. Stan apteczki: %d/%d.\n", get_time_str(), pharmacist_id, medicine_count, MAX_MEDICINE_CAPACITY);
    if (medicine_count < MEDICINE_PER_CONSULTATION) {
        printf("[%s] - Farmaceuta(%d): Budzę lekarza (mało leków).\n", get_time_str(), pharmacist_id);
        pthread_cond_signal(&doctor_cond);
    } else if (medicine_count < MAX_MEDICINE_CAPACITY &&
               !(patients_in_waiting_room == PATIENTS_PER_CONSULTATION && medicine_count >= MEDICINE_PER_CONSULTATION)) {
        printf("[%s] - Farmaceuta(%d): Budzę lekarza (może przyjąć dostawę, a nie pacjentów).\n", get_time_str(), pharmacist_id);
        pthread_cond_signal(&doctor_cond);
    }
    pthread_cond_wait(&pharmacist_delivery_accepted_cond, &hospital_mutex);
    printf("[%s] - Farmaceuta(%d): Zakończyłem dostawę.\n", get_time_str(), pharmacist_id);

    pthread_mutex_unlock(&hospital_mutex);
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Sposób użycia: %s <liczba_pacjentow> <liczba_farmaceutow>\n", argv[0]);
        return 1;
    }

    total_patients_arg = atoi(argv[1]);
    int num_pharmacists = atoi(argv[2]);

    if (total_patients_arg <= 0 || num_pharmacists <= 0) {
        fprintf(stderr, "Liczba pacjentów i farmaceutów musi być dodatnia.\n");
        return 1;
    }
     if (total_patients_arg % PATIENTS_PER_CONSULTATION != 0) {
        fprintf(stderr, "Ostrzeżenie: Liczba pacjentów (%d) nie jest wielokrotnością %d. Ostatni niepełna grupa może nie zostać obsłużona.\n", total_patients_arg, PATIENTS_PER_CONSULTATION);
    }

    srand(time(NULL));
    pthread_mutex_init(&hospital_mutex, NULL);
    pthread_cond_init(&doctor_cond, NULL);
    pthread_cond_init(&patient_can_enter_cond, NULL);
    pthread_cond_init(&pharmacist_can_deliver_cond, NULL);
    pthread_cond_init(&consultation_slots_filled_cond, NULL);
    pthread_cond_init(&pharmacist_delivery_accepted_cond, NULL);

    patient_consultation_finished_cond = malloc(total_patients_arg * sizeof(pthread_cond_t));
    if (patient_consultation_finished_cond == NULL) {
        perror("malloc dla patient_consultation_finished_cond");
        return 1;
    }
    for (int i = 0; i < total_patients_arg; ++i) {
        pthread_cond_init(&patient_consultation_finished_cond[i], NULL);
    }


    pthread_t doctor_tid;
    pthread_t patient_tids[total_patients_arg];
    pthread_t pharmacist_tids[num_pharmacists];
    pthread_create(&doctor_tid, NULL, doctor_thread_func, NULL);
    for (int i = 0; i < total_patients_arg; ++i) {
        int* patient_id = malloc(sizeof(int));
        *patient_id = i; 
        pthread_create(&patient_tids[i], NULL, patient_thread_func, patient_id);
    }
    for (int i = 0; i < num_pharmacists; ++i) {
        int* pharmacist_id = malloc(sizeof(int));
        *pharmacist_id = i; 
        pthread_create(&pharmacist_tids[i], NULL, pharmacist_thread_func, pharmacist_id);
    }
    pthread_join(doctor_tid, NULL);
    printf("[%s] - Główny: Lekarz zakończył pracę.\n", get_time_str());

    pthread_mutex_lock(&hospital_mutex);
    for(int i=0; i < total_patients_arg; ++i) {
        pthread_cond_signal(&patient_consultation_finished_cond[i]);
    }
    pthread_mutex_unlock(&hospital_mutex);


    for (int i = 0; i < total_patients_arg; ++i) {
        pthread_join(patient_tids[i], NULL);
    }
    printf("[%s] - Główny: Wszyscy pacjenci zakończyli.\n", get_time_str());

    pthread_mutex_lock(&hospital_mutex);
    pthread_cond_broadcast(&pharmacist_can_deliver_cond);
    pthread_cond_signal(&pharmacist_delivery_accepted_cond);
    pthread_mutex_unlock(&hospital_mutex);


    for (int i = 0; i < num_pharmacists; ++i) {
        pthread_join(pharmacist_tids[i], NULL);
    }
    printf("[%s] - Główny: Wszyscy farmaceuci zakończyli.\n", get_time_str());
    pthread_mutex_destroy(&hospital_mutex);
    pthread_cond_destroy(&doctor_cond);
    pthread_cond_destroy(&patient_can_enter_cond);
    pthread_cond_destroy(&pharmacist_can_deliver_cond);
    pthread_cond_destroy(&consultation_slots_filled_cond);
    pthread_cond_destroy(&pharmacist_delivery_accepted_cond);

    for (int i = 0; i < total_patients_arg; ++i) {
        pthread_cond_destroy(&patient_consultation_finished_cond[i]);
    }
    free(patient_consultation_finished_cond);

    printf("[%s] - Główny: Sprzątanie zakończone. Koniec programu.\n", get_time_str());
    return 0;
}