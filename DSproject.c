#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>

#define MAX_LINE 80

int main(void) {
    char *args[MAX_LINE/2 + 1];
    char input[MAX_LINE];
    char last_command[MAX_LINE] = ""; 
    int should_run = 1;

    while (should_run) {
        printf("uinxsh> ");
        fflush(stdout);

        // 1. Clean up zombie processes
        while (waitpid(-1, NULL, WNOHANG) > 0);

        if (fgets(input, MAX_LINE, stdin) == NULL) break;
        input[strcspn(input, "\n")] = '\0';

        // 2. History Management (!!)
        if (strcmp(input, "!!") == 0) {
            if (strlen(last_command) == 0) {
                printf("No commands in history.\n");
                continue;
            }
            strcpy(input, last_command);
            printf("%s\n", input);
        } else {
            strcpy(last_command, input);
        }

        // 3. Parsing input into arguments
        int i = 0;
        char *token = strtok(input, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (args[0] == NULL) continue;

        // 4. Built-in Commands
        if (strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        }

        if (strcmp(args[0], "pwd") == 0) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) printf("%s\n", cwd);
            continue;
        }

        if (strcmp(args[0], "cd") == 0) {
            if (args[1] != NULL) {
                if (chdir(args[1]) != 0) perror("cd failed");
            }
            continue;
        }

        if (strcmp(args[0], "help") == 0) {
            printf("Supported commands: cd, pwd, exit, clear, help, !!\n");
            printf("Parallel project: mont_carlo <procs> <points>\n");
            printf("Pipe: command1 | command2\n");
            continue;
        }

        if (strcmp(args[0], "clear") == 0) {
            printf("\033[H\033[J");
            continue;
        }

        // 5. Parallel Project: Monte Carlo Pi Estimation
        if (strcmp(args[0], "mont_carlo") == 0) {
            if (args[1] == NULL || args[2] == NULL) {
                printf("Usage: mont_carlo <processes> <points>\n");
                continue;
            }
            int n_procs = atoi(args[1]);
            long t_pts = atol(args[2]);
            
            // Create shared memory for inter-process communication
            long *global_count = mmap(NULL, sizeof(long), PROT_READ|PROT_WRITE, 
                                     MAP_SHARED|MAP_ANONYMOUS, -1, 0);
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
            
            double pi_val = 4.0 * (*global_count) / t_pts;
            printf("Estimated Pi with %d processes: %f\n", n_procs, pi_val);
            munmap(global_count, sizeof(long));
            continue;
        }

        // 6. Pipe Support (|)
        int pipe_pos = -1;
        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], "|") == 0) {
                pipe_pos = j;
                args[j] = NULL;
                break;
            }
        }

        if (pipe_pos != -1) {
            int fd[2];
            if (pipe(fd) == -1) { perror("Pipe failed"); continue; }

            if (fork() == 0) { // First child
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]); close(fd[1]);
                execvp(args[0], args);
                exit(1);
            }
            if (fork() == 0) { // Second child
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]); close(fd[0]);
                execvp(args[pipe_pos + 1], &args[pipe_pos + 1]);
                exit(1);
            }
            close(fd[0]); close(fd[1]);
            wait(NULL); wait(NULL);
            continue;
        }

        // 7. Background Execution (&)
        int bg = 0;
        if (i > 0 && args[i-1] != NULL && strcmp(args[i-1], "&") == 0) {
            bg = 1;
            args[i-1] = NULL;
        }

        // 8. External Command Execution
        pid_t pid = fork();
        if (pid == 0) {
            if (execvp(args[0], args) == -1) {
                printf("Command not found: %s\n", args[0]);
                exit(1);
            }
        } else if (pid > 0) {
            if (!bg) {
                waitpid(pid, NULL, 0);
            } else {
                printf("[Background Process PID: %d]\n", pid);
            }
        } else {
            perror("Fork failed");
        }
    }
    return 0;
}