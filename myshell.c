#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int prepare(void)
{
	return 0;
}

int process_arglist(int count, char **arglist) {
	int flag, son_exit_status;

	if (strcmp(arglist[count], "&") == 0) { // process needs to run in the background
		arglist[count] = NULL; // not send "&" symbol to son
		int pid = fork();
		if (pid == 0) { // son executes the command
			execvp(arglist[0], arglist);
		}
		else { // parent doesn't wait for son to finish
			
		}
	}
	if (strcmp(arglist[count-1], ">") == 0) { // command has output redirecting
		// TODO
	}
	else {
		for (int i = 0; i < count; i++) {
			if (strcmp(arglist[i], "|") == 0) { // command has a pipe
				// TODO
				flag = 1;
				break;
			}
		}
		if (flag == 0) { // regular single command
			int pid = fork();
			if (pid == 0) { // son executes the command
				execvp(arglist[0], arglist);
			}
			else { // parent waits for son to finish
				waitpid(pid, &son_exit_status, 0);
			}
		}	
	}
}

int finalize(void)
{
	return 0;
}

int main(void)
{
}
