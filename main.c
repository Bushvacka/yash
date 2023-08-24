#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

int getPathDirectories(char***);

int main() {
	// bool running = true;
	// char line[2000] = {0};


	// while (running) {
	// 	printf("# "); // Print shell prompt
	// 	gets(line, sizeof(line));

	// 	if (line[0] == 0) { // Catch EOF characters
	// 		running = false
	// 	} else { // Parse commands
		
	// 	}

	// 	memset(line, 0, sizeof(line)); // Clear previous command
	// }

	char** path_dirs;
	int num_dirs = getPathDirectories(&path_dirs);

	for (int i = 0; i < num_dirs; i++) {
		DIR* dir = opendir(path_dirs[i]);
	
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, "top") == 0) {
				printf("File found in %s: %s\n", path_dirs[i], entry->d_name);
				closedir(dir);
			}
    	}
	}
}

int getPathDirectories(char*** path_dirs) {
	// Stores directories found in the PATH environment variable in the provided pointer
	// Returns -1 if retrieving directories failed, otherwise the number of directories
	char* path = getenv("PATH");

	if (path == NULL) {return -1;} // Failed

	// Count number of path directories
	int num_dirs = 1;
	for (int i = 0; path[i]; i++) {
		if (path[i] == ':') {num_dirs++;}
	}

	// Allocate an array to store directories
	*path_dirs = (char**) malloc(num_dirs * sizeof(char*));


	// Split path into individual directories
	char* token = strtok(path, ":");

	// Store directories
	int i = 0;
	while (token != NULL) {
		(*path_dirs)[i] = (char*) malloc(sizeof(token));
		strcpy((*path_dirs)[i], token);
		printf("%s\n", (*path_dirs)[i]);
		i++;
		token = strtok(NULL, ":");
	}

	return num_dirs;
}