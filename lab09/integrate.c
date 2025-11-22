#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

#define M_PI 3.14159265358979323846

#define MAX_THREADS 100  // definiujemy zeby nie robic mallocow

// elementy z lab6
double range_left = 0.0;
double range_right = 1.0;

// Funkcja do całkowania
double f(double x) {
    return 4.0 / (x * x + 1.0); 
}

double calculate_integral(double interval_start, double interval_stop, double (*fun)(double), 
                         double interval_width, unsigned long long number_of_intervals) {
    if (interval_stop - interval_start < interval_width)
        return fun((interval_start + interval_stop) / 2.0) * (interval_stop - interval_start);
    
    double sum = 0.0;
    for (unsigned long long i = 0; i < number_of_intervals; i++) {
        sum += fun((interval_start + interval_width / 2.0));
        interval_start += interval_width;
    }
    
    return sum * interval_width;
}


// tablice wynikow i gotowosci
double results[MAX_THREADS];
int ready_flags[MAX_THREADS] = {0};

// struktura ktora bedziemy przekazywac dane do danego watku
typedef struct {
    double interval_start;
    double interval_stop;
    double interval_width;
    unsigned long long intervals_count;
    int thread_id;
} ThreadData;


// obsluga pracy danego watku
void* thread_work(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    // obliczanie calki dla danego fragmentu
    double result = calculate_integral(
        data->interval_start,
        data->interval_stop,
        f,
        data->interval_width,
        data->intervals_count
    );
    
    results[data->thread_id] = result; // zapis do tablic
    ready_flags[data->thread_id] = 1;
    
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc < 3) {return -1;}
    
    // argumenty
    double interval_width = strtod(argv[1], NULL);
    long num_of_threads = strtol(argv[2], NULL, 10);

    if (num_of_threads > MAX_THREADS) {return -1;}
    
    // liczba przedzialow
    unsigned long long total_intervals_count = (unsigned long long)ceil((range_right - range_left) / interval_width);
    unsigned long long intervals_per_thread = total_intervals_count / num_of_threads; // per watek
    
    pthread_t threads[MAX_THREADS];// id watkow

    ThreadData thread_data_array[MAX_THREADS]; // dane do watkow
    
    double interval_start = range_left;
    double interval_stop;
    
    // watki i przedzielanie obliczen
    for (int i = 0; i < num_of_threads; i++) {
        // koniec przedzial
        if (range_right < interval_start + intervals_per_thread * interval_width) {
            interval_stop = range_right;
        } else {
            interval_stop = interval_start + intervals_per_thread * interval_width;
        }
        
        // przygotowywanie dancych do watkow
        thread_data_array[i].interval_start = interval_start;
        thread_data_array[i].interval_stop = interval_stop;
        thread_data_array[i].interval_width = interval_width;
        thread_data_array[i].intervals_count = intervals_per_thread;
        thread_data_array[i].thread_id = i;
        
        // tworzymy watek
        if (pthread_create(&threads[i], NULL, thread_work, (void*)&thread_data_array[i]) != 0) {
            perror("Błąd podczas tworzenia wątku");
            exit(EXIT_FAILURE);
        }
        
        interval_start = interval_stop;
    }
    

    int all_ready;
    do {
        all_ready = 1;
        for (int i = 0; i < num_of_threads; i++) {
            if (ready_flags[i] == 0) {
                all_ready = 0;
                break;
            }
        }
        if (!all_ready) {usleep(1000);}
    } while (!all_ready);
    
    // sumowanie
    double sum = 0.0;
    for (int i = 0; i < num_of_threads; i++) {
        sum += results[i];
    }
    
    printf("Wartość całki: %lf\n", sum);
    
    return 0;
}