#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>

#define MAX_LINE 80
#define TOTAL_POINTS 1000000 
#define NUM_PROCESSES 4

int main(void) {
    char *args[MAX_LINE/2 + 1];
    char input[MAX_LINE];
    char last_command[MAX_LINE] = ""; 
    int should_run = 1;

    while (should_run) {
        printf("uinxsh> ");
        fflush(stdout);

        while (waitpid(-1, NULL, WNOHANG) > 0);

        if (fgets(input, MAX_LINE, stdin) == NULL) break;
        input[strcspn(input, "\n")] = '\0';


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


        int i = 0;
        char *token = strtok(input, " ");
        while (token != NULL) {
            args[i] = token;
            i++;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (args[0] == NULL) continue;


        if (strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        }

        if (strcmp(args[0], "pwd") == 0) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            }
            continue;
        }

        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                fprintf(stderr, "uinxsh: expected argument to \"cd\"\n");
            } else {
                if (chdir(args[1]) != 0) {
                    perror("uinxsh");
                }
            }
            continue;
        }

        if (strcmp(args[0], "help") == 0) {
            printf("\n--- UinxShell Help ---\n");
            printf("Built-in commands available:\n");
            printf("  cd <path>  : Change the current directory\n");
            printf("  pwd        : Print current working directory\n");
            printf("  clear      : Clear the terminal screen\n");
            printf("  exit       : Terminate the shell\n");
            printf("  help       : Display this information\n");
            printf("  !!         : Run the last command again\n");
            printf("External commands (like ls, mkdir, etc.) are also supported.\n\n");
            continue;
        }

        if (strcmp(args[0], "clear") == 0) {
            printf("\033[H\033[J");
            continue;
        }

        // Monte Carlo Pi 
        if (strcmp(args[0], "estimate_pi") == 0) {
            int *circle_count = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
                                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            *circle_count = 0;
            int pts_per_proc = TOTAL_POINTS / NUM_PROCESSES;

            for (int p = 0; p < NUM_PROCESSES; p++) {
                if (fork() == 0) {
                    srand(time(NULL) ^ (getpid() << 16));
                    int local = 0;
                    for (int j = 0; j < pts_per_proc; j++) {
                        double x = (double)rand() / RAND_MAX;
                        double y = (double)rand() / RAND_MAX;
                        if (x*x + y*y <= 1.0) local++;
                    }
                    *circle_count += local;
                    exit(0);
                }
            }
            for (int p = 0; p < NUM_PROCESSES; p++) wait(NULL);
            
            double pi = 4.0 * (*circle_count) / TOTAL_POINTS;
            printf("Pi Estimation using %d processes: %f\n", NUM_PROCESSES, pi);
            munmap(circle_count, sizeof(int));
            continue;
        }

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
            pipe(fd);
            if (fork() == 0) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]); close(fd[1]);
                execvp(args[0], args);
                exit(1);
            }
            if (fork() == 0) {
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]); close(fd[0]);
                execvp(args[pipe_pos + 1], &args[pipe_pos + 1]);
                exit(1);
            }
            close(fd[0]); close(fd[1]);
            wait(NULL); wait(NULL);
            continue;
        }


        int run_in_background = 0;
        if (i > 0 && args[i-1] != NULL && strcmp(args[i-1], "&") == 0) {
            run_in_background = 1;
            args[i-1] = NULL;
        }

    }
    return 0;
}