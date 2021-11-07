#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int prepare(void)
{
	return 0;
}

int process_arglist(int count, char **arglist) {
	int flag, son_exit_status;

	if (strcmp(arglist[count], "&") == 0) { // process needs to run in the background
		int pid = fork();
		if (pid == 0) { // son executes the command
			arglist[count] = NULL; // not send "&" symbol to execvp
			execvp(arglist[0], arglist);
		}
		// parent does not wait for son to finish
	}

	else if (strcmp(arglist[count-1], ">") == 0) { // command has output redirecting
		char *filename = arglist[count];
		int fd = open(filename, O_WRONLY | O_CREAT, 00777);
		int pid = fork();
		if (pid == 0) { // son executes th command
			arglist[count-1] = NULL; // not send ">" symbol and filename to execvp
			dup2(fd, 1); // make standard output of son to be fd
			execvp(arglist[0], arglist);
		}
		else { // parent waits for son to finish
			waitpid(pid, &son_exit_status, 0);
		}
	}

	else {
		for (int i = 0; i < count; i++) {
			if (strcmp(arglist[i], "|") == 0) { // command has a pipe
				int pipefds[2]; // array for fds from pipe 
				int pipe_status = pipe(pipefds);
				if (pipe_status == -1) {
					printf("Error creating pipe: %s\n", strerror(errno));
    				return 0;
				}
				int pid1 = fork();
				if (pid1 == 0) { // first son executes the first command
					close(pipefds[0]); // closing reading pd for writing son
					arglist[i] = NULL; // first son only gets first command
					dup2(pipefds[1], 1); // make standard output of first son to be fd1
					execvp(arglist[0], arglist);
				}
				else { // parent
					waitpid(pid1, &son_exit_status, 0); // waits for first son to finish
					int pid2 = fork();
					if (pid2 == 0) { // second son executes the second command
						close(pipefds[1]); // closing writing pd for reading son
						arglist[i] = arglist[0]; // copy name to the first argument of the second command
						dup2(pipefds[0], 0); // make standard input of second son to be fd0
						execvp(arglist[i], &arglist[i]); // only sending the second command
					}
					else {
						waitpid(pid2, &son_exit_status, 0); // parent waits for second son to finish
						close(pipefds[0]);
						close(pipefds[1]);
					}
				}
				flag = 1;
				break;
			}
		}
		// TODO add errors where needed

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
	return 1;
}

int finalize(void)
{
	return 0;
}