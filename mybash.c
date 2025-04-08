#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_ARGS 64
#define MAX_COMMANDS 10
#define MAX_INPUT 1024

struct Command {
    char *args[MAX_ARGS];
    char *input_file;
    char *output_file;
    int background;
};

// Разделение строки на команды по конвейерам
int parser_pipeline(char *input, struct Command *commands) {
    char *token;
    int cmd_count = 0;
    
    token = strtok(input, "|");
    while (token != NULL && cmd_count < MAX_COMMANDS) {
        commands[cmd_count].input_file = NULL;
        commands[cmd_count].output_file = NULL;
        commands[cmd_count].background = 0;
        
        int arg_count = 0;
        char *arg = strtok(token, " \t\n");
        
        while (arg != NULL && arg_count< MAX_ARGS - 1){
            if (strcmp(arg, "<") == 0) {
                arg = strtok(NULL, " \t\n");
                if (arg != NULL) {
                    commands[cmd_count].input_file = strdup(arg);
                }
            } 
            else if (strcmp(arg, ">")== 0) {
                arg = strtok(NULL, " \t\n");
                if (arg != NULL){
                    commands[cmd_count].output_file = strdup(arg);
                }
            }
            else if (strcmp(arg, "&") == 0){
                commands[cmd_count].background = 1;
            }
            else {
                commands[cmd_count].args[arg_count++] = strdup(arg);
            }
            
            arg = strtok(NULL, " \t\n");
        }
        
        commands[cmd_count].args[arg_count] = NULL;
        cmd_count++;
        token = strtok(NULL, "|");
    }
    
    return cmd_count;
}

void free_commands(struct Command *commands, int count){
    for (int i = 0; i < count; i++) {
        for (int j = 0; commands[i].args[j] != NULL; j++) {
            free(commands[i].args[j]);
        }
        if (commands[i].input_file) free(commands[i].input_file);
        if (commands[i].output_file) free(commands[i].output_file);
    }
}

void execute_command(struct Command *cmd) {
    if (cmd->input_file) {
        int fd = open(cmd -> input_file, O_RDONLY);
        if (fd < 0) {
            perror("Open input file");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->output_file) {
        int fd = open(cmd -> output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("Open output file");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    execvp(cmd->args[0], cmd -> args);
    perror("Execvp");
    exit(EXIT_FAILURE);
}

void execute_pipeline(struct Command *commands, int cmd_count) {
    int i;
    int prev_pipe = -1;
    int pipefd[2];
    pid_t pid;

    for (i = 0; i < cmd_count; i++) {
        if (i < cmd_count - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { 
            if (prev_pipe != -1){
                dup2(prev_pipe, STDIN_FILENO);
                close(prev_pipe);
            }

            if (i <cmd_count - 1){
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            execute_command(&commands[i]);
        } 
            else {

            if (prev_pipe!= -1) {
                close(prev_pipe);
            }

            if (i < cmd_count - 1) {
                close(pipefd[1]);
                prev_pipe = pipefd[0];
            }

            if (!commands[i].background) {
                waitpid(pid, NULL, 0);
            }
        }
    }
}

int main() {
    char input[MAX_INPUT];
    struct Command commands[MAX_COMMANDS];
    
    while (1) {
        printf("mybash--> ");
        fflush(stdout);
        
        if (!fgets(input, MAX_INPUT, stdin)) {
            break;
        }
        
        if (strcmp(input, "exit\n") == 0){
            break;
        }
        
        int cmd_count = parser_pipeline(input, commands);
        if (cmd_count == 0) {
            continue;
        }
        
        execute_pipeline(commands, cmd_count);
        
        free_commands(commands, cmd_count);
    }
    
    return 0;
}
