#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STRING_LENGTH 1024


char** load_strings(const char* filename, int* count, int* max_pattern_length) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int capacity = 100;
    char** strings = (char**)malloc(capacity * sizeof(char*));
    char buffer[MAX_STRING_LENGTH];
    *count = 0;
    *max_pattern_length = 0;

    while (fgets(buffer, MAX_STRING_LENGTH, file)) {
        if (*count >= capacity) {
            capacity *= 2;
            strings = (char**)realloc(strings, capacity * sizeof(char*));
        }
        buffer[strcspn(buffer, "\r\n")] = 0; // Remove newline
        strings[*count] = strdup(buffer);
        int len = strlen(buffer);
        if (len > *max_pattern_length) {
            *max_pattern_length = len;
        }
        (*count)++;
    }

    fclose(file);
    return strings;
}

int is_continuous_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int continuous = 1;
    char ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == ' ' || ch == '\n') {
            continuous = 0;
            break;
        }
    }

    fclose(file);
    return continuous;
}

void quick_search(const char* text, long text_length, const char* pattern, int pattern_length, long start_offset, int rank) {
    for (long i = 0; i <= text_length - pattern_length; i++) {
        if (strncmp(&text[i], pattern, pattern_length) == 0) {
            long global_position = start_offset + i;
            printf("Process %d found '%s' at position %ld\n", rank, pattern, global_position);
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) {
        if (rank == 0) {
            fprintf(stderr, "Usage: %s <strings_to_search.txt> <large_file.txt>\n", argv[0]);
        }
        MPI_Finalize();
        exit(1);
    }

    const char* search_file = argv[1];
    const char* large_file = argv[2];

    double start_time = MPI_Wtime();

    int num_patterns, max_pattern_length;
    char** patterns = NULL;
    if (rank == 0) {
        patterns = load_strings(search_file, &num_patterns, &max_pattern_length);
    }

    MPI_Bcast(&num_patterns, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&max_pattern_length, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        patterns = (char**)malloc(num_patterns * sizeof(char*));
        for (int i = 0; i < num_patterns; i++) {
            patterns[i] = (char*)malloc(MAX_STRING_LENGTH * sizeof(char));
        }
    }
    for (int i = 0; i < num_patterns; i++) {
        MPI_Bcast(patterns[i], MAX_STRING_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);
    }

    int continuous = 0;
    if (rank == 0) {
        continuous = is_continuous_file(large_file);
    }
    MPI_Bcast(&continuous, 1, MPI_INT, 0, MPI_COMM_WORLD);

    FILE* file = fopen(large_file, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", large_file);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    long chunk_size = file_size / size;
    long overlap = continuous ? (max_pattern_length - 1) : 0;
    long start_offset = rank * chunk_size;
    long end_offset = (rank == size - 1) ? file_size : start_offset + chunk_size;

    if (rank > 0 && continuous) start_offset -= overlap;
    long buffer_size = end_offset - start_offset;

    char* buffer = (char*)malloc(buffer_size + 1);
    fseek(file, start_offset, SEEK_SET);
    fread(buffer, 1, buffer_size, file);
    buffer[buffer_size] = '\0';
    fclose(file);


    if (!continuous) {
        for (long i = buffer_size - 1; i >= 0; i--) {
            if (buffer[i] == '\n') {
                buffer[i + 1] = '\0';
                break;
            }
        }
    }


    for (int i = 0; i < num_patterns; i++) {
        char* pattern = patterns[i];
        int pattern_length = strlen(pattern);
        quick_search(buffer, buffer_size, pattern, pattern_length, start_offset, rank);
    }

    free(buffer);
    for (int i = 0; i < num_patterns; i++) {
        free(patterns[i]);
    }
    free(patterns);

    MPI_Barrier(MPI_COMM_WORLD);


    MPI_Finalize();
    double end_time = MPI_Wtime();
    if (rank == 0) {
        printf("Execution Time: %.6f seconds\n", end_time - start_time);
    }
    return 0;
}
