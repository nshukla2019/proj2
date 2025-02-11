/* doit.C
 *
 * Joseph Petitti - CS 3013 Project 1
 *
 *
 * This project implements a command shell that accepts Linux commands from a user,
 * forks off a child process, and executes the command in the child. It also prints
 * out usage statistics for the process after completion.
 *
 * The project can be compiled with the 'make' command, and executed by running the
 * file 'doit'. The 'doit' program takes another command as an agrument and executes
 * that command. For instance, executing:
 *
 * % ./doit wc foo.txt
 *
 * would invoke the 'wc' (word count) command with an argument of 'foo.txt'. After
 * execution of the specified command has completed, doit will display statistics
 * that show some of the resources the command used.
 *
 * The shell includes several built-in commands:
 * 	exit - causes the shell to terminate
 * 	cd dir - causes the shell to change the directory to dir
 * 	set prompt = newprompt - causes the shell to change the prompt to newprompt
 *
 * This shell mode also supports running tasks in the background, indicated by
 * appending an ampersand character, '&', to the argument list. The 'jobs' command
 * will show a list of all running background tasks. If the user tries to exit the
 * shell with the 'exit' command while there are still background tasks running, the
 * shell will wait for the background tasks to complete before exiting.
 *
 * This simple shell is powerful, but still lacks many of the features common in
 * regular Linux shells. There is no tab completion, and it doesn't store previously
 * used commands. It also does not support many of the customization options of
 * regular Linux shells, such as command aliases.
 *
*/

#include <iostream>
using namespace std;
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <vector>
#define MAX_CHARS 128
#define MAX_ARGS 32

/* struct for storing processes */
typedef struct {
	int pid;
	string cmd;
	long startTime;
} process;

char *prompt = (char *)malloc(16 * sizeof(char));
int ampersand = 0; // set if a process is running in the background
vector<process> children;


void printStats(long start_ms) {
	struct rusage usage;
	struct timeval wc_end, ut_end, st_end;
	gettimeofday(&wc_end, NULL);
	long end_ms = (wc_end.tv_sec * 1000) + (wc_end.tv_usec / 1000);
	getrusage(RUSAGE_CHILDREN, &usage);
	ut_end = usage.ru_utime;
	st_end = usage.ru_stime;
	double userTime = ((ut_end.tv_sec * 1000) + (ut_end.tv_usec / 1000));
	double systemTime = ((st_end.tv_sec * 1000) + (st_end.tv_usec / 1000));
	double wallClockTime = end_ms - start_ms;
	cout << "USAGE STATISTICS:\n";
	cout << "*** system time = " << systemTime << " milliseconds\n";
	cout << "*** user time = " << userTime << " milliseconds\n";
	cout << "*** wall clock time = " << wallClockTime << " milliseconds\n";
	cout << "*** involuntary context switches = " << usage.ru_nivcsw << endl;
	cout << "*** voluntary context switches = " << usage.ru_nvcsw << endl;
	cout << "*** page faults requiring I/O = " << usage.ru_majflt << endl;
	cout << "*** page faults serviced without I/O = " << usage.ru_minflt << endl;
	cout << "*** maximum resident set size = " << usage.ru_maxrss << " kilobytes\n";
}

void safeExit() {
	if (children.size() > 0) { // wait for children for finish
		cout << "Waiting for " << children.size() << " process(es) to finish" << endl;
		for (unsigned long i = 0; i < children.size(); i++) {
			int status;
			pid_t result = waitpid(children.at(i).pid, &status, 0);
			if (result > 0) { // child finished
				cout << "[" << i + 1 << "] " << children.at(i).pid << " Completed\n";
				printStats(children.at(i).startTime);
			}
		}
	}
	exit(0);
}

int execute(char ** newArgs) {
	int pid;
	long start_ms;
	struct timeval wc_start;

	gettimeofday(&wc_start, NULL);
	start_ms = ((wc_start.tv_sec * 1000) + (wc_start.tv_usec / 1000));

	/* fork it */
	pid = fork();

	if (pid < 0) {
		cerr << "Fork error\n";
		exit(1);
	} else if (pid == 0) {
		/* child process */
		/* execute the command */
		if (execvp(newArgs[0], newArgs) < 0) {
			cerr << "execvp error\n";
			exit(1);
		}
		return 0;
	} else {
		/* parent */
		/* wait for child to finish */
		if (!ampersand) {
			wait(0);
			printStats(start_ms);
			return 0;
		} else {
			/* background process */
			process child = {pid, newArgs[0], start_ms};
			children.push_back(child);
			cout << "[" << children.size() << "] " << children.back().pid << endl;
			return 0;
		}
	}
}

/* grabs a line from cin and executes it. */
int doLine(char **out) {
	char line[MAX_CHARS];
	char *token;
	int position = 0;

	cin.getline(line, MAX_CHARS);
	token = strtok(line, " ");
	while (token != NULL) {
		out[position] = token;
		token = strtok(NULL, " ");
		position++;
	}

	if (strcmp(out[position - 1], "&") == 0) { // background process
		ampersand = 1;
		out[position - 1] = NULL;
	} else {
		ampersand = 0;
		out[position] = NULL;
	}
	if (strcmp(out[0], "exit") == 0) { // exit command
		safeExit();
		return 1;
	} else if (strcmp(out[0], "cd") == 0 && out[1] != NULL)  { // cd command
		if (chdir(out[1]) != 0)
			cerr << "chdir error\n";
	} else if (strcmp(out[0], "set") == 0 && strcmp(out[1], "prompt") == 0 && strcmp(out[2], "=") == 0 && out[3] != NULL) { // set prompt command
		strcpy(prompt, out[3]);
	} else if (strcmp(out[0], "jobs") == 0) {
		if (children.size() == 0) {
			cout << "No jobs running\n";
		} else {
			for (unsigned long i = 0; i < children.size(); i++) {
				cout << "[" << i + 1 << "] " << children[i].pid << " " << children[i].cmd << endl;
			}
		}
	} else {
		execute(out);
	}

	return 0;
}



int main(int argc, char *argv[]) {
	char **newArgs = (char **)malloc(MAX_ARGS * sizeof(char *));
	int i;
	prompt[0] = '=';
	prompt[1] = '=';
	prompt[2] = '>';
	prompt[3] = '\0';

	/* if there are command line arguments */
	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			newArgs[i - 1] = argv[i];
		}
		newArgs[argc - 1] = NULL;
		execute(newArgs);
	} else { // enter shell mode
		while (1) {
			// print prompt
			cout << prompt << " ";

			char line[MAX_CHARS];
			char *token;
			int position = 0;

			cin.getline(line, MAX_CHARS); // read in line

			// check for finishing background processes
			for (unsigned long j = 0; j < children.size(); j++) {
				int childStatus;
				pid_t result = waitpid(children.at(j).pid, &childStatus, WNOHANG);
				if (result > 0) { // child quit
					cout << "[" << j + 1 << "] " << children.at(j).pid << " Completed\n";
					printStats(children.at(j).startTime);
					children.erase(children.begin() + j);
				}
			}

			// tokenize the input line
			token = strtok(line, " ");
			while (token != NULL) {
				newArgs[position] = token;
				token = strtok(NULL, " ");
				position++;
			}

			if (strcmp(newArgs[position - 1], "&") == 0) { // background process
				ampersand = 1;
				newArgs[position - 1] = NULL;
			} else {
				ampersand = 0;
				newArgs[position] = NULL;
			}
			if (strcmp(newArgs[0], "exit") == 0) { // exit command
				safeExit();
			} else if (strcmp(newArgs[0], "cd") == 0 && newArgs[1] != NULL)  { // cd command
				if (chdir(newArgs[1]) != 0)
					cerr << "chdir error\n";
			} else if (strcmp(newArgs[0], "set") == 0 && strcmp(newArgs[1], "prompt") == 0 && strcmp(newArgs[2], "=") == 0 && newArgs[3] != NULL) { // set prompt command
				strcpy(prompt, newArgs[3]);
			} else if (strcmp(newArgs[0], "jobs") == 0) {
				if (children.size() == 0) {
					cout << "No jobs running\n";
				} else {
					for (unsigned long i = 0; i < children.size(); i++) {
						cout << "[" << i + 1 << "] " << children[i].pid << " " << children[i].cmd << endl;
			}
				}
			} else {
				execute(newArgs);
			}
			//doLine(newArgs);
		}
	}
}
