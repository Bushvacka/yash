#ifndef YASH
#define YASH

#define MAX_JOBS 20
#define MAX_LINE_LENGTH 2000

typedef struct
{
	// Job information
	pid_t pgid;
	bool foreground;
	int job_number;
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

int getMaxJobNumber();

int getMostRecentStoppedJob();

void bringJobToForeground(Job job);

void resumeStoppedJob(Job job);

void printJobTable();

void printJob();

#endif