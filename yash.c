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

#define MAX_JOBS 20
#define MAX_LINE_LENGTH 200

char running[] = "Running";
char stopped[] = "Stopped";
char done[] = "Done";

typedef struct
{
	// Job information
	pid_t pgid;
	bool foreground;
	char* status; // Running, Stopped, Done
	char command[MAX_LINE_LENGTH];

	// Process information
	int num_commands;
	char** commands[2];
	int command_length[2];
	int input_fd[2];
	int output_fd[2];
	int error_fd[2];
} Job;

Job parseLine(char* line);

int splitString(char* str, const char* delimiter, char*** tokens);

void executeJob(Job job);

void freeJob(Job job);

void printJobTable();

// Foreground job
pid_t fg_pid = -1;
Job fg_job;


// Background jobs
Job jobs[MAX_JOBS];
int job_index = 0;

int main() {
	char line[MAX_LINE_LENGTH] = {0};

	bool done = false;

	while (!done) {
		printf("# "); // Print shell prompt
		fgets(line, sizeof(line), stdin); // Get a line from the user
		
		if (line[0] == '\0') { // Catch EOF characters
			done = true;
		} else {
			// Remove newline character from the end of the line
			for (int i = 0; line[i]; i++) {
				if (line[i] == '\n' && line[i + 1] == '\0') {
					line[i--] = '\0';
				}
			}

			if (strcmp(line, "fg") == 0) {
				kill(jobs[job_index - 1].pgid, SIGCONT);
				jobs[job_index - 1].status = running;
			} else if (strcmp(line, "bg") == 0) {

			} else if (strcmp(line, "jobs") == 0) {
				printJobTable();
			} else { // General POSIX command
				Job job = parseLine(line);
				
				executeJob(job);

			}
		}

		memset(line, 0, sizeof(line)); // Clear previous command
	}

	// Free all jobs
	for (int i = 0; i < job_index; i++) {
		freeJob(jobs[i]);
	}
}

Job parseLine(char* line) {
	Job job;

	strcpy(job.command, line); // Store line for job table display
	job.status = running;
	job.foreground = true;

	// Check for an '&' indicating a background process
	for (int i = 0; line[i]; i++) {
		if (line[i] == '&' && line[i + 1] == '\0') {
			job.foreground = false;
			line[i--] = '\0';
		}
	}

	// Split the line on a pipe
	char** commands;
	int num_commands = splitString(line, "|", &commands);
	job.num_commands = num_commands;

	int pipe_fd[2]; // 0 - Read, 1 - Write
	if (num_commands == 2) {
		pipe(pipe_fd);
	}

	for (int i = 0; i < num_commands; i++) {
		// Split the line on every whitepsace
		char** command_tokens;
		int num_command_tokens = splitString(commands[i], " ", &command_tokens);

		// Set default file descriptors
		job.input_fd[i] = (i == 1 && num_commands == 2) ? pipe_fd[0] : STDIN_FILENO;
		job.output_fd[i] = (i == 0 && num_commands == 2) ? pipe_fd[1] : STDOUT_FILENO;
		job.error_fd[i] = STDERR_FILENO;

		int cmd_length = num_command_tokens;

		// Check for redirection symbols
		for (int j = 0; j < num_command_tokens; j++) {
			if (strcmp(command_tokens[j], "<") == 0) { // Input
				job.input_fd[i] = open(command_tokens[j + 1], O_RDONLY);

				if (job.input_fd[i] == -1) {
					perror("input");
				} else if (j < cmd_length) {
					cmd_length = j;
				}
			} else if (strcmp(command_tokens[j], ">") == 0) { // Output
				job.output_fd[i] = open(command_tokens[j + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
				
				if (job.output_fd[i] == -1) {
					perror("output");
				} else if (j < cmd_length) {
					cmd_length = j;
				}
			} else if (strcmp(command_tokens[j], "2>") == 0) { // Error
				job.error_fd[i] = open(command_tokens[j + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

				if (job.error_fd[i] == -1) {
					perror("error");
				} else if (j < cmd_length) {
					cmd_length = j;
				}
			}
		}

		// Extract command arguments from tokens
		job.commands[i] = (char**) malloc((cmd_length + 1) * sizeof(char*));
		for (int j = 0; j < cmd_length; j++) {
			job.commands[i][j] = (char*) malloc(strlen(command_tokens[j]) * sizeof(char));
			strcpy(job.commands[i][j], command_tokens[j]);
		}
		job.commands[i][cmd_length] = NULL;

		job.command_length[i] = cmd_length;
		
		// Free command tokens
		for (int j = 0; j < num_command_tokens; j++) {
			free(command_tokens[j]);
		}
		free(command_tokens);
	}

	// Free commands
	for (int i = 0; i < num_commands; i++) {
		free(commands[i]);
	}
	free(commands);


	return job;
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

void executeJob(Job job) {
	for (int i = 0; i < job.num_commands; i++) {
		pid_t pid = fork();

		if (pid == -1) { // Catch fork errors
			perror("fork");
			return;
		} 

		if (i == 0) { // Save job pgid
			job.pgid = pid;
		}

		if (pid == 0) { // Child Process
			if (i == 0) { // First command
				setpgid(0, 0); // Create a process group
			} else { // Second command
				setpgid(0, job.pgid); // Join the process group
			}

			// printf("Process Group: %d\n", getpgrp());

			// Redirect file descriptors
			dup2(job.input_fd[i], STDIN_FILENO);
			dup2(job.output_fd[i], STDOUT_FILENO);
			dup2(job.error_fd[i], STDERR_FILENO);

			// Execute command
			execvp(job.commands[i][0], job.commands[i]);

			// Execvp does not return when successful
			perror("exec");
			exit(1);
		} else { // Parent process
			if (i + 1 == job.num_commands) { // Final command
				if (job.foreground) {
					// Save foreground process information
					fg_job = job;
					fg_pid = job.pgid;

					signal(SIGTTOU, SIG_IGN);

					// Give control of the terminal to the child process
					tcsetpgrp(STDIN_FILENO, job.pgid);

					pid_t status = waitpid(job.pgid, NULL, WUNTRACED);

					// printf("Status: %d\n", status);

					// Return control of the terminal to the shell
					tcsetpgrp(STDIN_FILENO, getpgrp());

					// Mark foreground process as completed
					fg_pid = -1;
					freeJob(fg_job);

					// Close file descriptors if they were changed
					for (int i = 0; i < job.num_commands; i++) {
						if (job.input_fd[i] != STDIN_FILENO) {
							close(job.input_fd[i]);
						}
						if (job.output_fd[i] != STDOUT_FILENO) {
							close(job.output_fd[i]);
						}
						if (job.error_fd[i] != STDERR_FILENO) {
							close(job.error_fd[i]);
						}
					}

				} else { // Background
					jobs[job_index++] = job; // Add to job table
				}  
			}
		}

	}
}

void freeJob(Job job) {
	for (int i = 0; i < job.num_commands; i++) {
		for (int j = 0; j < job.command_length[i]; j++) {
			free(job.commands[i][j]);
		}
		free(job.commands[i]);
	}
}

void printJobTable() {
	for (int i = 0; i < job_index; i++) {
		printf("[%d] %c %s %-10s\n", i + 1, (i + 1 == job_index) ? '+' : '-', jobs[i].status, jobs[i].command);
	}
}
