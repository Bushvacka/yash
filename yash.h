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
	char *status; // Running, Stopped, Done
	char command[MAX_LINE_LENGTH + 1];

	// Process information
	int num_commands;
	char **commands[2];
	int command_length[2];
	int input_fd[2];
	int output_fd[2];
	int error_fd[2];
} Job;

// Creates a job from user input
Job parseLine(char *line);

// Seperate string into tokens based on the provided delimiter
// Stores result in tokens and returns number of tokens
int splitString(char *str, const char *delimiter, char ***tokens);

// Start a foreground or background job
void executeJob(Job job);

// Returns the highest job number present in the job table
int getMaxJobNumber();

// Brings the most recent stopped or background job to the foreground
void bringJobToForeground(int job_index);

// Returns the index of the most recent stopped job
int getMostRecentStoppedJob();

// Resumes a stopped job in the background
void resumeStoppedJob(int job_index);

// Prints any finished background processes to the terminal before removing them from the job table
void updateJobTable();

// Display a list of background jobs
void printJobTable();

// Display a job
void printJob();

// Free the memory associated with a job
void freeJob(Job job);

// Remove a job from the job table
void removeJob(int job_index);

#endif
