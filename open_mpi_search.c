#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STRING_LENGTH 1024

// Function to load strings (patterns) from a file
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

// Function to search for a pattern in multiple directions (forwards and backwards)
void search_in_matrix(char** matrix, int rows, int cols, const char* pattern, int pattern_length, int rank) {
    // Search horizontally (left to right) in each row
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j <= cols - pattern_length; j++) {
            if (strncmp(&matrix[i][j], pattern, pattern_length) == 0) {
                printf("Process %d found '%s' horizontally (forward) at position (%d, %d)\n", rank, pattern, i, j);
            }
        }

        // Search horizontally (right to left) in each row
        for (int j = cols - 1; j >= pattern_length - 1; j--) {
            if (strncmp(&matrix[i][j - pattern_length + 1], pattern, pattern_length) == 0) {
                printf("Process %d found '%s' horizontally (backward) at position (%d, %d)\n", rank, pattern, i, j - pattern_length + 1);
            }
        }
    }

    // Search vertically (top to bottom) in each column
    for (int j = 0; j < cols; j++) {
        for (int i = 0; i <= rows - pattern_length; i++) {
            int match = 1;
            for (int k = 0; k < pattern_length; k++) {
                if (matrix[i + k][j] != pattern[k]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Process %d found '%s' vertically (forward) at position (%d, %d)\n", rank, pattern, i, j);
            }
        }

        // Search vertically (bottom to top) in each column
        for (int i = rows - 1; i >= pattern_length - 1; i--) {
            int match = 1;
            for (int k = 0; k < pattern_length; k++) {
                if (matrix[i - k][j] != pattern[k]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Process %d found '%s' vertically (backward) at position (%d, %d)\n", rank, pattern, i - pattern_length + 1, j);
            }
        }
    }

    // Search diagonally (top-left to bottom-right)
    for (int i = 0; i <= rows - pattern_length; i++) {
        for (int j = 0; j <= cols - pattern_length; j++) {
            int match = 1;
            for (int k = 0; k < pattern_length; k++) {
                if (matrix[i + k][j + k] != pattern[k]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Process %d found '%s' diagonally (TL to BR) at position (%d, %d)\n", rank, pattern, i, j);
            }
        }
    }

    // Search diagonally (top-right to bottom-left)
    for (int i = 0; i <= rows - pattern_length; i++) {
        for (int j = pattern_length - 1; j < cols; j++) {
            int match = 1;
            for (int k = 0; k < pattern_length; k++) {
                if (matrix[i + k][j - k] != pattern[k]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Process %d found '%s' diagonally (TR to BL) at position (%d, %d)\n", rank, pattern, i, j);
            }
        }
    }

    // Search diagonally (bottom-left to top-right)
    for (int i = pattern_length - 1; i < rows; i++) {
        for (int j = 0; j <= cols - pattern_length; j++) {
            int match = 1;
            for (int k = 0; k < pattern_length; k++) {
                if (matrix[i - k][j + k] != pattern[k]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Process %d found '%s' diagonally (BL to TR) at position (%d, %d)\n", rank, pattern, i, j);
            }
        }
    }

    // Search diagonally (bottom-right to top-left)
    for (int i = pattern_length - 1; i < rows; i++) {
        for (int j = pattern_length - 1; j < cols; j++) {
            int match = 1;
            for (int k = 0; k < pattern_length; k++) {
                if (matrix[i - k][j - k] != pattern[k]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Process %d found '%s' diagonally (BR to TL) at position (%d, %d)\n", rank, pattern, i, j);
            }
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
            fprintf(stderr, "Usage: %s <patterns_file.txt> <matrix_file.txt>\n", argv[0]);
        }
        MPI_Finalize();
        exit(1);
    }

    const char* search_file = argv[1];
    const char* matrix_file = argv[2];

    // Start timer
    double start_time = MPI_Wtime();

    int num_patterns, max_pattern_length;
    char** patterns = NULL;
    if (rank == 0) {
        patterns = load_strings(search_file, &num_patterns, &max_pattern_length);
    }

    // Broadcast the number of patterns and max pattern length to all processes
    MPI_Bcast(&num_patterns, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&max_pattern_length, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Broadcast patterns to all processes
    if (rank != 0) {
        patterns = (char**)malloc(num_patterns * sizeof(char*));
        for (int i = 0; i < num_patterns; i++) {
            patterns[i] = (char*)malloc(MAX_STRING_LENGTH * sizeof(char));
        }
    }
    for (int i = 0; i < num_patterns; i++) {
        MPI_Bcast(patterns[i], MAX_STRING_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);
    }

    // Open the matrix file and load it into memory
    FILE* file = fopen(matrix_file, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", matrix_file);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Get the number of rows and columns in the matrix
    int rows = 0, cols = 0;
    char buffer[MAX_STRING_LENGTH];
    while (fgets(buffer, MAX_STRING_LENGTH, file)) {
        if (cols == 0) {
            cols = strlen(buffer) - 1; // Remove newline character
        }
        rows++;
    }
    rewind(file);

    // Allocate memory for the matrix
    char** matrix = (char**)malloc(rows * sizeof(char*));
    for (int i = 0; i < rows; i++) {
        matrix[i] = (char*)malloc(cols * sizeof(char));
    }

    // Read the matrix into memory
    int row = 0;
    while (fgets(buffer, MAX_STRING_LENGTH, file)) {
        buffer[strcspn(buffer, "\r\n")] = 0; // Remove newline
        strncpy(matrix[row], buffer, cols);
        row++;
    }
    fclose(file);

    // Each process searches for each pattern in the matrix
    for (int i = 0; i < num_patterns; i++) {
        search_in_matrix(matrix, rows, cols, patterns[i], strlen(patterns[i]), rank);
    }

    // Clean up
    for (int i = 0; i < num_patterns; i++) {
        free(patterns[i]);
    }
    free(patterns);

    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);

    // End timer and log total execution time
    double end_time = MPI_Wtime();
    if (rank == 0) {
        printf("Execution Time: %.6f seconds\n", end_time - start_time);
    }

    MPI_Finalize();
    return 0;
}
