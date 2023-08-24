#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int main() {
	bool running = true;
	char line[2000] = {0};


	while (running) {
		printf("# "); // Print shell prompt
		gets(line, sizeof(line));

		if (line[0] == 0) { // Catch EOF characters
			running = false
		} else {

		}

		memset(line, 0, sizeof(line)); // Clear previous command
	}
}

char** getPathDirectories() {
	char* path = getenv("PATH");

	// Count number of path directories
	int num_dirs = 1;
	for (int i = 0; path[i]; i++) {
		if (path[i] == ':') {num_dirs++;}
	}

	// Allocate an array to store directories
	char** path_dirs = (char**) malloc(num_dirs * sizeof(char*));

	// Split path into individual directories
	char* token = strtok(path, ":");

	int i = 0;
	while (token != NULL) {
		path_dirs[i] = (char*) malloc(sizeof(token));
		strcpy(path_dirs[i], token);
		i++;
		token = strtok(NULL, ":");
	}

	return path_dirs;
}