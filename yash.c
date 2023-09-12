#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "yash.h"

char running[] = "Running";
char stopped[] = "Stopped";
char done[] = "Done";

// Background jobs
Job jobs[MAX_JOBS];
int num_jobs = 0;

int main() {
    // Parent ignores SIGINT & SIGTSTP
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // Ignore SIGTTOU signal (Sent when terminal control is transferred)
    signal(SIGTTOU, SIG_IGN);

    char line[MAX_LINE_LENGTH + 1] = {0};

    bool done = false;

    while (!done) {
        fflush(stdout);
        printf("# ");                     // Print shell prompt
        fgets(line, sizeof(line), stdin); // Get a line from the user

        if (line[0] == '\0') { // Catch EOF characters (Ctrl+D)
            done = true;
        } else {
            // Remove newline character from the end of the line
            for (int i = 0; line[i]; i++) {
                if (line[i] == '\n' && line[i + 1] == '\0') {
                    line[i--] = '\0';
                }
            }

            // Handle terminated processes
            updateJobTable();

            if (strcmp(line, "fg") == 0) {
                bringJobToForeground(num_jobs - 1);
            } else if (strcmp(line, "bg") == 0) {
                int stopped_job_index = getMostRecentStoppedJob();

                if (stopped_job_index != -1) {
                    resumeStoppedJob(stopped_job_index);
                }
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
    for (int i = 0; i < num_jobs; i++) {
        freeJob(jobs[i]);
    }
}

Job parseLine(char *line) {
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
    char **commands;
    int num_commands = splitString(line, "|", &commands);
    job.num_commands = num_commands;

    int pipe_fd[2]; // 0 - Read, 1 - Write
    if (num_commands == 2) {
        pipe(pipe_fd);
    }

    for (int i = 0; i < num_commands; i++) {
        // Split the line on every whitepsace
        char **command_tokens;
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
                job.output_fd[i] = open(command_tokens[j + 1], O_WRONLY | O_CREAT | O_TRUNC,
                                        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

                if (job.output_fd[i] == -1) {
                    perror("output");
                } else if (j < cmd_length) {
                    cmd_length = j;
                }
            } else if (strcmp(command_tokens[j], "2>") == 0) { // Error
                job.error_fd[i] = open(command_tokens[j + 1], O_WRONLY | O_CREAT | O_TRUNC,
                                       S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

                if (job.error_fd[i] == -1) {
                    perror("error");
                } else if (j < cmd_length) {
                    cmd_length = j;
                }
            }
        }

        // Extract command arguments from tokens
        job.commands[i] = (char **)malloc((cmd_length + 1) * sizeof(char *));
        for (int j = 0; j < cmd_length; j++) {
            job.commands[i][j] = (char *)malloc(strlen(command_tokens[j]) + 1);
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

int splitString(char *str, const char *delimiter, char ***tokens) {
    int num_tokens = 0;

    // Allocate an array to store tokens
    *tokens = (char **)malloc(sizeof(char *));

    // Get first token
    char *token = strtok(str, delimiter);

    while (token != NULL) {
        num_tokens++;
        // Resize to accomodate the token
        *tokens = (char **)realloc(*tokens, num_tokens * sizeof(char *));

        // Save token
        (*tokens)[num_tokens - 1] = (char *)malloc(strlen(token) + 1);
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
            // Child does not ignore SIGINT & SIGTSTP
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            // Redirect file descriptors
            dup2(job.input_fd[i], STDIN_FILENO);
            dup2(job.output_fd[i], STDOUT_FILENO);
            dup2(job.error_fd[i], STDERR_FILENO);

            // Close unused file descriptors
            if (job.num_commands == 2) {
                int j = (i == 0) ? 1 : 0; // Choose other command

                if (job.input_fd[j] != STDIN_FILENO) {
                    close(job.input_fd[j]);
                }
                if (job.output_fd[j] != STDOUT_FILENO) {
                    close(job.output_fd[j]);
                }
                if (job.error_fd[j] != STDERR_FILENO) {
                    close(job.error_fd[j]);
                }
            }

            // Execute command
            execvp(job.commands[i][0], job.commands[i]);

            // If this point is reached - execvp failed

            // Free job memory and exit
            for (int j = 0; j < num_jobs; j++) {
                freeJob(jobs[j]);
            }
            freeJob(job);

            exit(1);
        } else { // Parent process
            if (i == 0) {
                setpgid(pid, pid); // Create a process group
            }
            if (i + 1 == job.num_commands) // Final command
            {
                setpgid(pid, job.pgid); // Join the process group

                // Close file descriptors if they were changed
                for (int j = 0; j < job.num_commands; j++) {
                    if (job.input_fd[j] != STDIN_FILENO) {
                        close(job.input_fd[j]);
                    }
                    if (job.output_fd[j] != STDOUT_FILENO) {
                        close(job.output_fd[j]);
                    }
                    if (job.error_fd[j] != STDERR_FILENO) {
                        close(job.error_fd[j]);
                    }
                }

                if (job.foreground) {

                    // Give control of the terminal to the child process
                    tcsetpgrp(STDIN_FILENO, job.pgid);
                    tcsetpgrp(STDOUT_FILENO, job.pgid);
                    tcsetpgrp(STDERR_FILENO, job.pgid);

                    // Wait for job to finish
                    int status;
                    for (int j = 0; j < job.num_commands; j++) {
                        waitpid(-job.pgid, &status, WUNTRACED);
                    }

                    // Return control of the terminal to the shell
                    tcsetpgrp(STDIN_FILENO, getpgrp());
                    tcsetpgrp(STDOUT_FILENO, getpgrp());
                    tcsetpgrp(STDERR_FILENO, getpgrp());

                    if (WIFEXITED(status) || (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)) { // Process exited normally or was stopped by Ctrl+C
                        freeJob(job);
                    } else if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTSTP) { // Process was stopped by Ctrl+Z - add job to job table
                        job.job_number = getMaxJobNumber() + 1;
                        job.status = stopped;
                        jobs[num_jobs++] = job;
                    }
                } else { // Background - add job to job table
                    job.job_number = getMaxJobNumber() + 1;
                    jobs[num_jobs++] = job;
                }
            }
        }
    }
}

int getMaxJobNumber() {
    if (num_jobs == 0) {
        return 0;
    }

    return jobs[num_jobs - 1].job_number;
}

void bringJobToForeground(int job_index) {
    if (job_index < 0) {
        return;
    }

    printf("%s\n", jobs[job_index].command); // Print command to terminal

    if (jobs[job_index].status == stopped) {
        kill(-jobs[job_index].pgid, SIGCONT); // Send continue signal to process group
        jobs[job_index].status = running;     // Update job status
    };

    // Give control of the terminal to the process
    tcsetpgrp(STDIN_FILENO, jobs[job_index].pgid);
    tcsetpgrp(STDOUT_FILENO, jobs[job_index].pgid);
    tcsetpgrp(STDERR_FILENO, jobs[job_index].pgid);

    // Wait for job to finish
    int status;
    int result;
    do {
        result = waitpid(-jobs[job_index].pgid, &status, WUNTRACED);
    } while (result > 0 && !(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTSTP));

    // Return control of the terminal to the shell
    tcsetpgrp(STDIN_FILENO, getpgrp());
    tcsetpgrp(STDOUT_FILENO, getpgrp());
    tcsetpgrp(STDERR_FILENO, getpgrp());

    if (result == -1 && errno == ECHILD) { // Process exited or was killed - remove from job list
        freeJob(jobs[job_index]);
        removeJob(job_index);
    } else if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTSTP) { // Process was stopped by Ctrl+Z - update status
        jobs[job_index].status = stopped;
    }
}

int getMostRecentStoppedJob() {
    for (int i = num_jobs - 1; i >= 0; i--) {
        if (jobs[i].status == stopped) {
            return i;
        }
    }
    return -1; // No stopped jobs
}

void resumeStoppedJob(int job_index) {
    if (jobs[job_index].foreground) {
        printf("[%d]+ %s &\n", jobs[job_index].job_number, jobs[job_index].command); // Print command to terminal with & at end
    } else {
        printf("[%d]+ %s\n", jobs[job_index].job_number, jobs[job_index].command); // Print command to terminal
    }

    jobs[job_index].status = running; // Update job status

    kill(-jobs[job_index].pgid, SIGCONT); // Send continue signal to process group
}

void updateJobTable() {
    // Mark terminated jobs as done
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].status == running) {
            int status;
            int result;
            do {
                result = waitpid(-jobs[i].pgid, &status, WNOHANG);
            } while (result > 0 && WIFEXITED(status));

            if (result == -1 && errno == ECHILD) { // Child process no longer running
                jobs[i].status = done;
                printJob(jobs[i]);
            }
        }
    }

    // Remove any finished jobs
    for (int i = num_jobs - 1; i >= 0; i--) {
        if (jobs[i].status == done) {
            freeJob(jobs[i]);
            removeJob(i);
        }
    }
}

void printJobTable() {
    for (int i = 0; i < num_jobs; i++) {
        printJob(jobs[i]);
    }
}

void printJob(Job job) {
    printf("[%d]%c %s %-10s\n", job.job_number,
           job.job_number == getMaxJobNumber() ? '+' : '-', job.status,
           job.command);
}

void freeJob(Job job) {
    for (int i = 0; i < job.num_commands; i++) {
        for (int j = 0; j < job.command_length[i]; j++) {
            free(job.commands[i][j]);
        }
        free(job.commands[i]);
    }
}

void removeJob(int job_index) {
    for (int i = job_index; i < num_jobs - 1; i++) {
        jobs[i] = jobs[i + 1];
    }
    num_jobs--;
}