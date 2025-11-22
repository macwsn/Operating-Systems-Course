#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 1024

// Funkcja odwracająca linię
void reverse_line(char *line) {
    int len = strlen(line);
    for (int i = 0; i < len / 2; i++) {
        char temp = line[i];
        line[i] = line[len - i - 1];
        line[len - i - 1] = temp;
    }
}

// Funkcja przetwarzająca plik
void flip_file(const char *input_path, const char *output_path) {
    FILE *input_file = fopen(input_path, "r");
    if (!input_file) {
        perror("Nie udało się otworzyć pliku do odczytu");
        return;
    }

    FILE *output_file = fopen(output_path, "w");
    if (!output_file) {
        perror("Nie udało się otworzyć pliku do zapisu");
        fclose(input_file);
        return;
    }

    char line[MAX_LINE_LENGTH];

    // Czytamy plik linia po linii
    while (fgets(line, sizeof(line), input_file)) {
        // Usuwamy znak nowej linii, jeśli istnieje
        line[strcspn(line, "\n")] = 0;

        // Odwracamy linię
        reverse_line(line);

        // Zapisujemy zmodyfikowaną linię
        fprintf(output_file, "%s\n", line);
    }

    fclose(input_file);
    fclose(output_file);
}

// Funkcja przetwarzająca katalog
void process_directory(const char *src_dir, const char *dst_dir) {
    struct dirent *entry;
    DIR *dir = opendir(src_dir);
    
    if (!dir) {
        perror("Nie udało się otworzyć katalogu źródłowego");
        return;
    }

    // Tworzymy katalog wynikowy, jeśli nie istnieje
    struct stat st = {0};
    if (stat(dst_dir, &st) == -1) {
        if (mkdir(dst_dir, 0700) == -1) {
            perror("Nie udało się stworzyć katalogu wynikowego");
            closedir(dir);
            return;
        }
    }

    // Przeszukujemy katalog źródłowy
    while ((entry = readdir(dir)) != NULL) {
        // Sprawdzamy, czy to plik tekstowy
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".txt")) {
            char input_path[1024], output_path[1024];

            // Tworzymy pełne ścieżki do plików
            snprintf(input_path, sizeof(input_path), "%s/%s", src_dir, entry->d_name);
            snprintf(output_path, sizeof(output_path), "%s/%s", dst_dir, entry->d_name);

            // Przetwarzamy plik
            flip_file(input_path, output_path);
            printf("Przetworzono plik: %s\n", entry->d_name);
        }
    }

    closedir(dir);
}

int main() {
    char src_dir[1024], dst_dir[1024];

    // Pobieramy ścieżki do katalogów od użytkownika
    printf("Podaj ścieżkę do katalogu źródłowego: ");
    scanf("%s", src_dir);

    printf("Podaj ścieżkę do katalogu wynikowego: ");
    scanf("%s", dst_dir);

    // Przetwarzamy katalogi
    process_directory(src_dir, dst_dir);

    return 0;
}
