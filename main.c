#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

int splitString(char* str, char delimiter, char*** tokens);

int main() {
	bool running = true;
	char line[2000] = {0};


	while (running) {
		printf("# "); // Print shell prompt
		gets(line, sizeof(line));

		if (line[0] == 0) { // Catch EOF characters
			running = false;
		} else { // Parse commands
			char** tokens;
			splitString(line, ' ', &tokens);

			char* args[1] = {NULL};
			execvp(tokens[0], args);
		}

		memset(line, 0, sizeof(line)); // Clear previous command
	}
}

int splitString(char* str, char delimiter, char*** tokens) {
	// Count number of tokens
	int num_tokens = 1;
	for (int i = 0; str[i]; i++) {
		if (str[i] == delimiter) {str++;}
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