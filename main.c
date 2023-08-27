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

int splitString(char* str, char delimiter, char*** tokens);

pid_t child_pid = -1;

// SIGINT
void sigint_handler(int signum) {
    if (child_pid != -1) {
        // Kill the child process if it's running
        kill(child_pid, SIGTERM);
    }
}

int main() {
	bool running = true;
	char line[2000] = {0};

	signal(SIGINT, sigint_handler);


	while (running) {
		printf("# "); // Print shell prompt
		fgets(line, sizeof(line), stdin);
		
		int input_fd = STDIN_FILENO;
		int output_fd = STDOUT_FILENO;
		int error_fd = STDERR_FILENO;
		
		if (line[0] == 0) { // Catch EOF characters
			running = false;
		} else { // Parse commands
			// Remove newline
			for (int i = 0; line[i]; i++) {
				if (line[i] == '\n') {
					line[i--] = '\0';
				}
			}

			char** tokens;
			int num_tokens = splitString(line, ' ', &tokens);

			int cmd_length = num_tokens;
			
			// Check for redirection symbols
			for (int i = 0; i < num_tokens; i++) {
				if (strcmp(tokens[i], "<") == 0) { // Input
					input_fd = open(tokens[i + 1], O_RDONLY);

					if (input_fd == -1) {
						perror("yash");
					} else if (i < cmd_length) {
						cmd_length = i;
					}
				} else if (strcmp(tokens[i], ">") == 0) { // Output
					output_fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
					
					if (output_fd == -1) {
						perror("yash");
					} else if (i < cmd_length) {
						cmd_length = i;
					}
				} else if (strcmp(tokens[i], "2>") == 0) { // Error
					error_fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

					if (error_fd == -1) {
						perror("yash");
					} else if (i < cmd_length) {
						cmd_length = i;
					}
				}
			}

			char** args = (char**) malloc((cmd_length + 1) * sizeof(char*));
			for (int i = 0; i < cmd_length; i++) {
				args[i] = tokens[i];
			}
			args[num_tokens] = NULL;

			pid_t pid = fork();

			if (pid == 0) { // Child process
				// Redirect file descriptors
				dup2(input_fd, STDIN_FILENO);
				dup2(output_fd, STDOUT_FILENO);
				dup2(error_fd, STDERR_FILENO);

				// Execute command
				execvp(args[0], args);

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

			for (int i = 0; i < num_tokens; i++) {
				free(tokens[i]);
			}
			free(tokens);
			free(args);
		}

		memset(line, 0, sizeof(line)); // Clear previous command
	}
}

int splitString(char* str, char delimiter, char*** tokens) {
	// Count number of tokens
	int num_tokens = 1;
	for (int i = 0; str[i]; i++) {
		if (str[i] == delimiter) {num_tokens++;}
	}

	// Allocate an array to store directories
	*tokens = (char**) malloc(num_tokens * sizeof(char*));


	// Split string into individual tokens
	char* token = strtok(str, &delimiter);

	int i = 0;
	while (token != NULL) {
		(*tokens)[i] = (char*) malloc(sizeof(token));
		strcpy((*tokens)[i], token);
		i++;
		token = strtok(NULL, &delimiter);
	}

	return num_tokens;
}