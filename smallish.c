/**
 * A sample program for parsing a command line. If you find it useful,
 * feel free to adapt this code for Assignment 4.
 * Do fix memory leaks and any additional issues you find.
 */

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <errno.h>
//#include <sys/signal.h>


#define INPUT_LENGTH 	 2048
#define MAX_ARGS		 512

bool foreground_only_mode = false;


struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};


// credit for below function: https://canvas.oregonstate.edu/courses/2017605/assignments/10180226?return_to=https%3A%2F%2Fcanvas.oregonstate.edu%2Fcalendar%23view_name%3Dmonth%26view_start%3D2025-11-13
struct command_line *parse_input()
{
	char input[INPUT_LENGTH];
	struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, INPUT_LENGTH, stdin);

	// Tokenize the input
	char *token = strtok(input, " \n");
	while(token){
		if(!strcmp(token,"<")){
			curr_command->input_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,">")){
			curr_command->output_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,"&")){
			curr_command->is_bg = true;
		} else{
			curr_command->argv[curr_command->argc++] = strdup(token);
		}
		token=strtok(NULL," \n");
	}
	return curr_command;
}

/* Signal handler for SIGTSTP 
/ Adapted from: https://canvas.oregonstate.edu/courses/2017605/pages/exploration-signal-handling-api?module_item_id=25843653
*/
void handle_SIGTSTP(int signo){
    char *enter_msg = "\nEntering foreground-only mode (& is now ignored)\n";
    char *exit_msg= "\nExiting foreground-only mode\n";

    if (!foreground_only_mode) {
        write(STDOUT_FILENO, enter_msg, strlen(enter_msg));
        foreground_only_mode = true;
    } else {
        write(STDOUT_FILENO, exit_msg, strlen(exit_msg));
        foreground_only_mode = false;
    }
	write(STDOUT_FILENO, ":", 1);


}

//
int main()
{
	struct command_line *curr_command;

	// credit for below: https://canvas.oregonstate.edu/courses/2017605/pages/exploration-signal-handling-api?module_item_id=25843653
  	// Initialize SIGINT_action struct to be empty

	// save current status and child status
	int curr_stat = 0;
	int child_stat = 0;

	// Your shell, i.e., the parent process, must ignore SIGINT.
  	struct sigaction sig_parent = {0};
	memset(&sig_parent, 0, sizeof sig_parent);
	sig_parent.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sig_parent, NULL);
	
	// SIGTSTP handler
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	while(true)
	{

		// keep checking for active background activities
		pid_t done;
		int childStatus;
		while ((done = waitpid(-1, &childStatus, WNOHANG)) > 0) {

			if (WIFEXITED(childStatus)) {
				printf("background pid %d is done: exit value %d\n", done, WEXITSTATUS(childStatus));
			} 
			else if (WIFSIGNALED(childStatus)) {
				printf("background pid %d is done: terminated by signal %d\n", done, WTERMSIG(childStatus));
			}
			fflush(stdout);

		}

		curr_command = parse_input();

		// blank command IGNORE
		if (curr_command->argc == 0) {
			continue;
		}

		// ignore # (comment)
		if (curr_command->argv[0][0] == '#') {
			continue;
		}

		pid_t spawnpid, wpid;
		//pid_t spawnpid = -5;
		int intVal = 10;

		// BUILT-IN COMMANDS
		// exit
		if (strcmp(curr_command->argv[0], "exit") == 0){
			exit(1);
		}

		// cd
		if (strcmp(curr_command->argv[0], "cd") == 0){

			int result;

			// ignore background commands
			curr_command->is_bg = false;

			// By itself - with no arguments - it changes to the directory specified in the HOME environment variable
			if (curr_command->argc == 1) {
				result = chdir(getenv("HOME"));
				//printf("%s\n", getenv("HOME"));
			}
			else {
				result = curr_command->argc;
			}

			if (result == -1) {
				perror("cd");
			}
			else {
				// print the new directory
				char cwd[2048];
				getcwd(cwd, sizeof(cwd));
				printf("%s\n", cwd);
			}
			continue;

		}


		// status
		if (strcmp(curr_command->argv[0], "status") == 0){
			// store the exit status or terminating signal of the last foreground command
			// which is updated each time waitpid() is run on foreground child
			
			// ignore background commands
			curr_command->is_bg = false;

			if (curr_stat == 2 || curr_stat == 3 || curr_stat == 9 || curr_stat == 11)
			{
				printf("terminated by signal %d\n", curr_stat);
			} else {
				printf("exit value %d\n", curr_stat);
			}

			fflush(stdout);
			continue;
		}


		// 4. Whenever a non-built in command is received, the parent (i.e., smallsh) will fork off a child.

		// adapted from: https://canvas.oregonstate.edu/courses/2017605/pages/exploration-process-api-monitoring-child-processes?module_item_id=25843645
		// If fork is successful, the value of spawnpid will be 0 in the child
		// and will be the child's pid in the parent
		//printf("Parent process's pid = %d\n", getpid());

		pid_t childPid = fork();
		
		if(childPid == -1){
			perror("fork() failed!");
			exit(EXIT_FAILURE);

		} else if(childPid == 0){
			// Child process

			// Any children running as background processes must ignore SIGINT.
			// A child running as a foreground process must terminate itself when it receives SIGINT.
			struct sigaction sig_child = {0};
			memset(&sig_child, 0, sizeof sig_child);
			if (curr_command->is_bg == false) { // foreground
				sig_child.sa_handler = SIG_DFL; // default
			}
			// child is running in background, ignore it
			else if ( curr_command->is_bg ) {
				sig_child.sa_handler = SIG_IGN;
			}
			sigaction(SIGINT, &sig_child, NULL);

			// all children ignore SIGTSTP
			struct sigaction sigtstp_ignore = {0};
			sigtstp_ignore.sa_handler = SIG_IGN;
			sigaction(SIGTSTP, &sigtstp_ignore, NULL);

			// If a command fails because the shell could not find the command to run, then the shell will print an error message and set the exit status to 1

			// input file
			if (curr_command->input_file != NULL) {
				// An input file redirected via stdin must be opened for reading only
				int in_file = open(curr_command->input_file, O_RDONLY); 
				// if your shell cannot open the file for reading, it must print an error message and set the exit status to 1 (but don't exit the shell).
				if (in_file == -1) {
					printf("cannot open %s for input\n", curr_command->input_file);
					//WEXITSTATUS(1);
					fflush(stderr);
					exit(1);
				}
				
				dup2(in_file, STDIN_FILENO);
				close(in_file);
			}

			// output file
			if (curr_command->output_file != NULL) {

				// make file to write action output to 
				// Similarly, an output file redirected via stdout must be opened for writing only 
				int out_file = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); 

				// it must be truncated if it already exists or created if it does not exist 
				// If your shell cannot open the output file, it must print an error message and set the exit status to 1 (but don't exit the shell).
				if (out_file == -1) {
					printf("Cannot open output file\n");
					//WEXITSTATUS(1);
					fflush(stderr);
					exit(1);
				}

				dup2(out_file, STDOUT_FILENO);
				close(out_file);
				
			}

			if (execvp(curr_command->argv[0], curr_command->argv) == -1 ) {
				perror(curr_command->argv[0]);
				exit(1);
			}

		// credit: https://canvas.oregonstate.edu/courses/2017605/pages/exploration-process-api-monitoring-child-processes?module_item_id=25843645
		} else{
			// parent process
			//printf("Child's pid = %d\n", childPid);

			// wait for child if child is foreground process
			if (curr_command->is_bg == false) {
				childPid = waitpid(childPid, &childStatus, 0);
				
				//If a child foreground process is killed by a signal, the parent must immediately print out the number of the signal that killed it's foreground child process
				if (WIFSIGNALED(childStatus)) {
					printf("terminated by signal %d\n", WTERMSIG(childStatus));
					curr_stat = WTERMSIG(childStatus);
					fflush(stdout);
				}

				// also update current status if child exits normally
				else if(WIFEXITED(childStatus)){
					curr_stat = WTERMSIG(childStatus);
					//printf("terminated by signal %d\n", curr_stat);
					fflush(stdout);
				}
			}

			// only print background id if it IS the background and foreground only mode is not on
			else if (curr_command->is_bg == true && foreground_only_mode == false) {
				printf("background pid is %d\n", childPid);
				fflush(stdout);
			}

			// child is not a foreground process, don't wait for it
			if(WIFEXITED(childStatus)){
				//Child %d exited normally
				curr_stat = WEXITSTATUS(childStatus);
				fflush(stdout);

			} 
			else if (WIFSIGNALED(childStatus)) {
				// child did not exit normally
				printf("background pid %d is done: terminated by signal %d\n", childPid, WTERMSIG(childStatus));
				curr_stat = WEXITSTATUS(childStatus);
				fflush(stdout);

			}
		}
	}

	return EXIT_SUCCESS;
}