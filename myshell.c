#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int prepare(void)
{
	signal(SIGINT, SIG_IGN); // shell should not terminate upon SIGINT
	return 0;
}

int process_arglist(int count, char **arglist) {
	int flag, child_exit_status;

	if (strcmp(arglist[count], "&") == 0) { // process needs to run in the background
		int pid = fork();
		if (pid == 0) { // child executes the command
			arglist[count] = NULL; // not send "&" symbol to execvp
			execvp(arglist[0], arglist);
		}
		// parent does not wait for child to finish
	}

	else if (strcmp(arglist[count-1], ">") == 0) { // command has output redirecting
		signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
		char *filename = arglist[count];
		int fd = open(filename, O_WRONLY | O_CREAT, 00777);
		int pid = fork();
		if (pid == 0) { // child executes th command
			arglist[count-1] = NULL; // not send ">" symbol and filename to execvp
			dup2(fd, 1); // make standard output of child to be fd
			execvp(arglist[0], arglist);
		}
		else { // parent waits for child to finish
			waitpid(pid, &child_exit_status, 0);
		}
	}

	else {
		for (int i = 0; i < count; i++) {
			if (strcmp(arglist[i], "|") == 0) { // command has a pipe
				signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
				int pipefds[2]; // array for fds from pipe 
				int pipe_status = pipe(pipefds);
				if (pipe_status == -1) {
					printf("Error creating pipe: %s\n", strerror(errno));
    				return 0;
				}
				int pid1 = fork();
				if (pid1 == 0) { // first child executes the first command
					close(pipefds[0]); // closing reading pd for writing child
					arglist[i] = NULL; // first child only gets first command
					dup2(pipefds[1], 1); // make standard output of first child to be fd1
					execvp(arglist[0], arglist);
				}
				else { // parent
					waitpid(pid1, &child_exit_status, 0); // waits for first child to finish
					int pid2 = fork();
					if (pid2 == 0) { // second child executes the second command
						close(pipefds[1]); // closing writing pd for reading child
						arglist[i] = arglist[0]; // copy name to the first argument of the second command
						dup2(pipefds[0], 0); // make standard input of second child to be fd0
						execvp(arglist[i], &arglist[i]); // only sending the second command
					}
					else {
						waitpid(pid2, &child_exit_status, 0); // parent waits for second child to finish
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
			signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
			int pid = fork();
			if (pid == 0) { // child executes the command
				execvp(arglist[0], arglist);
			}
			else { // parent waits for child to finish
				waitpid(pid, &child_exit_status, 0);
			}
		}	
	}
	return 1;
}

int finalize(void)
{
	return 0;
}