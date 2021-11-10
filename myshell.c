#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int get_command_pipe_index(int count, char **arglist);
void exit(int status);

int prepare(void)
{
	signal(SIGINT, SIG_IGN); // shell should not terminate upon SIGINT
	return 0;
}

int process_arglist(int count, char **arglist) {
	int child_exit_status, exec_status, dup2_status;
	int i;

	if (strcmp(arglist[count-1], "&") == 0) { // process needs to run in the background
		int pid = fork();
		if (pid < 0) { // error in fork
			perror("Error in fork.");
			exit(1);
		}
		if (pid == 0) { // child executes the command
			arglist[count-1] = NULL; // not send "&" symbol to execvp
			exec_status = execvp(arglist[0], arglist);
			if (exec_status == -1) { // exec failed
				perror("Failed executing execvp.");
				exit(1);
			}
		}
		else { // parent does not wait for child to finish
			signal(SIGCHLD, SIG_IGN); // SIGCHLD is ignored, the child entry is deleted from the process table. no zombie.
		}
	}

	else if (strcmp(arglist[count-2], ">") == 0) { // command has output redirecting
		char *filename = arglist[count-1];
		int fd = open(filename, O_WRONLY | O_CREAT, 00777);
		if (fd < 0) { // open failed
			perror("Failed creating or opening file.");
			exit(1);
		}
		int pid = fork();
		if (pid < 0) { // error in fork
			perror("Error in fork.");
			exit(1);
		}
		if (pid == 0) { // child executes th command
			signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
			arglist[count-2] = NULL; // not send ">" symbol and filename to execvp
			dup2_status = dup2(fd, 1); // make standard output of child to be fd
			if (dup2_status == -1) { // error in dup2
				perror("Error in dup2.");
				exit(1);
			}
			exec_status = execvp(arglist[0], arglist);
			if (exec_status == -1) { // exec failed
				perror("Failed executing execvp.");
				exit(1);
			}
		}
		else { // parent waits for child to finish
			waitpid(pid, &child_exit_status, 0);
		}
	}

	else {
		i = get_command_pipe_index(count, arglist);
		if (i == -1) { // regular single command 
			int pid = fork();
			if (pid < 0) { // error in fork
				perror("Error in fork.");
				exit(1);
			}
			if (pid == 0) { // child executes the command
				signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
				exec_status = execvp(arglist[0], arglist);
				if (exec_status == -1) { // exec failed
					perror("Failed executing execvp.");
					exit(1);
				}
			}
			else { // parent waits for child to finish
				waitpid(pid, &child_exit_status, 0);
			}
		}
		else { // command has a pipe
			int pipefds[2]; // array for fds from pipe 
			int pipe_status = pipe(pipefds);
			if (pipe_status == -1) {
				perror("Error creating pipe.");
				return 0;
			}
			int pid1 = fork();
			if (pid1 < 0) { // error in fork
				perror("Error in fork.");
				exit(1);
			}
			if (pid1 == 0) { // first child executes the first command
				signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
				close(pipefds[0]); // closing reading pd for writing child
				arglist[i] = NULL; // first child only gets first command
				dup2_status = dup2(pipefds[1], 1); // make standard output of first child to be fd1
				if (dup2_status == -1) { // error in dup2
					perror("Error in dup2.");
					exit(1);
				}
				exec_status = execvp(arglist[0], arglist);
				if (exec_status == -1) { // exec failed
					perror("Failed executing execvp.");
					exit(1);
				}
			}
			else { // parent
				waitpid(pid1, &child_exit_status, 0); // waits for first child to finish
				int pid2 = fork();
				if (pid2 < 0) { // error in fork
					perror("Error in fork.");
					exit(1);
				}
				if (pid2 == 0) { // second child executes the second command
					signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
					close(pipefds[1]); // closing writing pd for reading child
					arglist[i] = arglist[0]; // copy name to the first argument of the second command
					dup2_status = dup2(pipefds[0], 0); // make standard input of second child to be fd0
					if (dup2_status == -1) { // error in dup2
						perror("Error in dup2.");
						exit(1);
					}
					exec_status = execvp(arglist[i], &arglist[i]); // only sending the second command
					if (exec_status == -1) { // exec failed
						perror("Failed executing execvp.");
						exit(1);
					}
				}
				else {
					waitpid(pid2, &child_exit_status, 0); // parent waits for second child to finish
					close(pipefds[0]);
					close(pipefds[1]);
				}
			}
		}
	}
	return 1;
}

int finalize(void)
{
	return 0;
}

int get_command_pipe_index(int count, char **arglist) {
	for (int i = 0; i < count; i++) {
		if (strcmp(arglist[i], "|") == 0) {
			return i;
		}
	}
	return -1;
}