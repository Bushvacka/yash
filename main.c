#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int splitString(char* str, char delimiter, char*** tokens);

int main() {
	bool running = true;
	char line[2000] = {0};


	while (running) {
		printf("# "); // Print shell prompt
		fgets(line, sizeof(line), stdin);

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
			
			char** args = (char**)malloc((num_tokens + 1) * sizeof(char*));
			for (int i = 0; i < num_tokens; i++) {
				args[i] = tokens[i];
			}
			args[num_tokens] = NULL;

			pid_t pid = fork();

			if (pid == 0) { // Child process
				execvp(args[0], args);

				perror("yash");
			} else { // Parent process
				wait(NULL);
			}

			// Free memory allocated for tokens and args
			for (int i = 0; i < num_tokens; i++) {
				free(tokens[i]);
			}
			free(tokens);
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