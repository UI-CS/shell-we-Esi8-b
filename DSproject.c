#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 80

int main(void) {
    char *args[MAX_LINE/2 + 1];
    char input[MAX_LINE];
    char last_command[MAX_LINE] = ""; 
    int should_run = 1;

    while (should_run) {
        printf("uinxsh> ");
        fflush(stdout);

        while (waitpid(-1, NULL, WNOHANG) > 0);

        if (fgets(input, MAX_LINE, stdin) == NULL) {
            break;
        }

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

        if (args[0] == NULL) {
            continue;
        }


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

        int run_in_background = 0;
        if (i > 0 && strcmp(args[i-1], "&") == 0) {
            run_in_background = 1;
            args[i-1] = NULL;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
        } 
        else if (pid == 0) {
            if (execvp(args[0], args) == -1) {
                printf("Command not found: %s\n", args[0]);
                exit(1);
            }
        } 
        else {
            if (run_in_background == 0) {
                waitpid(pid, NULL, 0);
            } else {
                printf("[Process running in background, PID: %d]\n", pid);
            }
        }
    }

    return 0;
}