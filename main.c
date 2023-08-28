#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

void executeCommand(char** args, int input_fd, int output_fd, int error_fd);

char** parseCommand(char** tokens, int num_tokens, int* input_fd, int* output_fd, int* error_fd);

int splitString(char* str, const char* delimiter, char*** tokens);

void sigint_handler(int signum);

void sigtstp_handler(int signum);

pid_t child_pid = -1;

int main() {
	// Assign signal handlers
	signal(SIGINT, sigint_handler);
	// signal(SIGTSTP, sigint_handler);

	const char whitespace[2] = " ";
	const char pipechar[2] = "|";

	bool running = true;
	char line[2000] = {0};

	while (running) {
		printf("# "); // Print shell prompt
		fgets(line, sizeof(line), stdin); // Get a line from the user
		
		if (line[0] == 0) { // Catch EOF characters
			running = false;
		} else {
			// Remove newline character from the end of the line
			for (int i = 0; line[i]; i++) {
				if (line[i] == '\n' && line[i + 1] == '\0') {
					line[i--] = '\0';
				}
			}

			// Split the line on a pipe
			char** cmds;
 			int num_cmds = splitString(line, pipechar, &cmds);

			int pipe_fd[2]; // 0 - Read, 1 - Write

			if (num_cmds == 2) {
				pipe(pipe_fd);
			}

			for (int i = 0; i < num_cmds; i++) {
				// Split the line on every whitepsace
				char** tokens;
				int num_tokens = splitString(cmds[i], whitespace, &tokens);

				// Set default file descriptors
				int input_fd = (i == 1 && num_cmds == 2) ? pipe_fd[0] : STDIN_FILENO;
				int output_fd = (i == 0 && num_cmds == 2) ? pipe_fd[1] : STDOUT_FILENO;
				int error_fd = STDERR_FILENO;

				// Parse command from tokens
				char** args = parseCommand(tokens, num_tokens, &input_fd, &output_fd, &error_fd);

				// Execute command
				executeCommand(args, input_fd, output_fd, error_fd);

				// Free tokens and args
				for (int j = 0; j < num_tokens; j++) {
					free(tokens[j]);
				}
				free(tokens);
				free(args);
			}
		}

		memset(line, 0, sizeof(line)); // Clear previous command
	}
}

void executeCommand(char** args, int input_fd, int output_fd, int error_fd) {
	pid_t pid = fork();
	if (pid == -1) {
		perror("yash");
	} else if (pid == 0) { // Child process
		// Redirect file descriptors
		dup2(input_fd, STDIN_FILENO);
		dup2(output_fd, STDOUT_FILENO);
		dup2(error_fd, STDERR_FILENO);

		// Execute command
		execvp(args[0], args);

		// If this point has been reached an error has occurred
		perror("yash");
	} else { // Parent process
		child_pid = pid;

		wait(NULL);

		child_pid = -1;
	}

	// Close file descriptors if they were changed
	if (input_fd != STDIN_FILENO) {
		close(input_fd);
	}
	if (output_fd != STDOUT_FILENO) {
		close(output_fd);
	}
	if (error_fd != STDERR_FILENO) {
		close(error_fd);
	}
}

char** parseCommand(char** tokens, int num_tokens, int* input_fd, int* output_fd, int* error_fd) {
	int cmd_length = num_tokens;

	// Check for redirection symbols
	for (int i = 0; i < num_tokens; i++) {
		if (strcmp(tokens[i], "<") == 0) { // Input
			*input_fd = open(tokens[i + 1], O_RDONLY);

			if (*input_fd == -1) {
				perror("yash");
			} else if (i < cmd_length) {
				cmd_length = i;
			}
		} else if (strcmp(tokens[i], ">") == 0) { // Output
			*output_fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
			
			if (*output_fd == -1) {
				perror("yash");
			} else if (i < cmd_length) {
				cmd_length = i;
			}
		} else if (strcmp(tokens[i], "2>") == 0) { // Error
			*error_fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

			if (*error_fd == -1) {
				perror("yash");
			} else if (i < cmd_length) {
				cmd_length = i;
			}
		}
	}

	// Extract command arguments from tokens
	char** args = (char**) malloc((cmd_length + 1) * sizeof(char*));
	for (int i = 0; i < cmd_length; i++) {
		args[i] = tokens[i];
	}
	args[cmd_length] = NULL;

	return args;
}

int splitString(char* str, const char* delimiter, char*** tokens) {
    int num_tokens = 0;

    // Allocate an array to store tokens
    *tokens = (char**)malloc(sizeof(char*));

	// Get first token
	char* token = strtok(str, delimiter);

    while (token != NULL) {
        num_tokens++;
        // Resize to accomodate the token
        *tokens = (char**)realloc(*tokens, num_tokens * sizeof(char*));

        // Save token
        (*tokens)[num_tokens - 1] = (char*)malloc(strlen(token) + 1);
        strcpy((*tokens)[num_tokens - 1], token);

		// Get next token
        token = strtok(NULL, delimiter);
    }

    return num_tokens;
}

void sigint_handler(int signum) {
    if (child_pid != -1) {
        // Kill the child process if it's running
        kill(child_pid, SIGINT);
    }
}