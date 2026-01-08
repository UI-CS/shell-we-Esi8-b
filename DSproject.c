#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_LINE 80
#define THRESHOLD 20 

/* Helper function to remove leading/trailing whitespace and hidden characters */
void trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

/* Merge function for the sorting algorithm */
void merge(int *array, int low, int mid, int high) {
    int n1 = mid - low + 1, n2 = high - mid;
    int *left = malloc(n1 * sizeof(int));
    int *right = malloc(n2 * sizeof(int));
    
    for (int i = 0; i < n1; i++) left[i] = array[low + i];
    for (int i = 0; i < n2; i++) right[i] = array[mid + 1 + i];
    
    int i = 0, j = 0, k = low;
    while (i < n1 && j < n2) array[k++] = (left[i] <= right[j]) ? left[i++] : right[j++];
    while (i < n1) array[k++] = left[i++];
    while (j < n2) array[k++] = right[j++];
    
    free(left); free(right);
}

/* Parallel Merge Sort implementation using Fork-Join and Shared Memory */
void parallel_merge_sort(int *array, int low, int high) {
    if (low < high) {
        /* Criterion: Use sequential sort for small chunks to reduce overhead */
        if (high - low + 1 <= THRESHOLD) {
            for (int i = low; i <= high; i++) {
                for (int j = i + 1; j <= high; j++) {
                    if (array[i] > array[j]) {
                        int temp = array[i]; array[i] = array[j]; array[j] = temp;
                    }
                }
            }
            return;
        }

        int mid = low + (high - low) / 2;
        pid_t pid = fork();
        
        if (pid == 0) {
            /* Child process handles the left half */
            parallel_merge_sort(array, low, mid);
            exit(0);
        } else if (pid > 0) {
            /* Parent process handles the right half */
            parallel_merge_sort(array, mid + 1, high);
            /* Join: Wait for child to complete */
            waitpid(pid, NULL, 0); 
            /* Combine halves */
            merge(array, low, mid, high);
        }
    }
}

int main(void) {
    char *args[MAX_LINE/2 + 1];
    char input[MAX_LINE];
    char last_command[MAX_LINE] = ""; 
    int should_run = 1;

    while (should_run) {
        printf("uinxsh> ");
        fflush(stdout);

        /* Clean up zombie processes */
        while (waitpid(-1, NULL, WNOHANG) > 0);

        if (fgets(input, MAX_LINE, stdin) == NULL) break;
        input[strcspn(input, "\n")] = '\0';

        /* History Management */
        if (strcmp(input, "!!") == 0) {
            if (strlen(last_command) == 0) {
                printf("No commands in history.\n");
                continue;
            }
            strcpy(input, last_command);
            printf("%s\n", input);
        } else if (strlen(input) > 0) {
            strcpy(last_command, input);
        }

        /* Parsing input into arguments */
        int i = 0;
        char *token = strtok(input, " ");
        while (token != NULL) {
            trim(token);
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (args[0] == NULL) continue;

        /* Built-in Shell Commands */
        if (strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        }

        /* Parallel Project: Fork-Join Merge Sort */
        if (strcmp(args[0], "fork_sort") == 0) {
            int size = 40; 
            int *shared_array = mmap(NULL, size * sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
            
            srand(time(NULL));
            printf("Generating numbers...\n");
            for(int j=0; j<size; j++) shared_array[j] = rand() % 100;
            
            printf("Sorting in parallel...\n");
            parallel_merge_sort(shared_array, 0, size - 1);
            
            printf("Sorted array: ");
            for(int j=0; j<size; j++) printf("%d ", shared_array[j]);
            printf("\n");
            
            munmap(shared_array, size * sizeof(int));
            continue; 
        }

        /* Parallel Project: Monte Carlo Pi Estimation */
        if (strcmp(args[0], "mont_carlo") == 0) {
            if (args[1] == NULL || args[2] == NULL) {
                printf("Usage: mont_carlo <processes> <points>\n");
                continue;
            }
            int n_procs = atoi(args[1]);
            long t_pts = atol(args[2]);
            long *global_count = mmap(NULL, sizeof(long), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
            *global_count = 0;

            for (int p = 0; p < n_procs; p++) {
                if (fork() == 0) {
                    unsigned int seed = time(NULL) ^ (getpid() << 16);
                    long local = 0;
                    for (long j = 0; j < t_pts/n_procs; j++) {
                        double x = (double)rand_r(&seed)/RAND_MAX*2.0-1.0;
                        double y = (double)rand_r(&seed)/RAND_MAX*2.0-1.0;
                        if (x*x + y*y <= 1.0) local++;
                    }
                    *global_count += local;
                    exit(0);
                }
            }
            for (int p = 0; p < n_procs; p++) wait(NULL);
            printf("Estimated Pi: %f\n", 4.0 * (*global_count) / t_pts);
            munmap(global_count, sizeof(long));
            continue; 
        }

        if (strcmp(args[0], "pwd") == 0) {
            char cwd[1024]; getcwd(cwd, sizeof(cwd)); printf("%s\n", cwd); continue;
        }
        
        if (strcmp(args[0], "cd") == 0) {
            if (args[1]) chdir(args[1]); continue;
        }

        if (strcmp(args[0], "clear") == 0) {
            printf("\033[H\033[J"); continue;
        }

        /* External Command Execution */
        pid_t pid = fork();
        if (pid == 0) {
            if (execvp(args[0], args) == -1) {
                printf("Command not found: %s\n", args[0]);
                exit(1);
            }
        } else if (pid > 0) {
            waitpid(pid, NULL, 0);
        }
    }
    return 0;
}