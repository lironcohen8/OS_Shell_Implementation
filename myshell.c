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
	int child_exit_status, exec_status, dup2_status, close_status, wait_finish_status;
	int pipe_index;

	if (strcmp(arglist[count-1], "&") == 0) { // process needs to run in the background
		int pid = fork();
		if (pid < 0) { // error in fork
			perror("Fork failed");
			exit(1);
		}
		if (pid == 0) { // child executes the command
			arglist[count-1] = NULL; // not send "&" symbol to execvp
			exec_status = execvp(arglist[0], arglist);
			if (exec_status == -1) { // exec failed
				perror("Execvp failed");
				exit(1);
			}
		}
		else { // parent does not wait for child to finish
			signal(SIGCHLD, SIG_IGN); // SIGCHLD is ignored, the child entry is deleted from the process table. no zombie.
		}
	}

	else if (count > 1 && strcmp(arglist[count-2], ">") == 0) { // command has output redirecting
		char *filename = arglist[count-1];
		int fd = open(filename, O_RDWR | O_CREAT, 00777);
		if (fd < 0) { // open failed
			perror("Open or create file failed");
			exit(1);
		}
		int pid = fork();
		if (pid < 0) { // error in fork
			perror("Fork failed");
			exit(1);
		}
		if (pid == 0) { // child executes the command
			signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
			arglist[count-2] = NULL; // not send ">" symbol and filename to execvp
			dup2_status = dup2(fd, 1); // make standard output of child to be file
			if (dup2_status == -1) { // error in dup2
				perror("Dup2 failed");
				exit(1);
			}
			close_status = close(fd); // closing file
			if (close_status == -1) { // close failed
				perror("File closing failed");
				exit(1);
			}
			exec_status = execvp(arglist[0], arglist);
			if (exec_status == -1) { // exec failed
				perror("Execvp failed");
				exit(1);
			}
		}
		else { // parent waits for child to finish
			wait_finish_status = waitpid(pid, &child_exit_status, 0);
			if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
				perror("Waitpid failed");
				exit(1);
			}
			close_status = close(fd); // closing file
			if (close_status == -1) { // close failed
				perror("File closing failed");
				exit(1);
			}
		}
	}

	else {
		pipe_index = get_command_pipe_index(count, arglist);
		if (pipe_index == -1) { // regular single command 
			int pid = fork();
			if (pid < 0) { // error in fork
				perror("Fork failed");
				exit(1);
			}
			if (pid == 0) { // child executes the command
				signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
				exec_status = execvp(arglist[0], arglist);
				if (exec_status == -1) { // exec failed
					perror("Execvp failed");
					exit(1);
				}
			}
			else { // parent waits for child to finish
				wait_finish_status = waitpid(pid, &child_exit_status, 0);
				if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
					perror("Waitpid failed");
					exit(1);
				}
			}
		}
		else { // command has a pipe
			int pipefds[2]; // array for fds from pipe 
			int pipe_status = pipe(pipefds);
			if (pipe_status == -1) {
				perror("Pipe creation failed");
				exit(1);
			}
			int pid1 = fork();
			if (pid1 < 0) { // error in fork
				perror("Fork failed");
				exit(1);
			}
			if (pid1 == 0) { // first child executes the first command
				signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
				close_status = close(pipefds[0]); // closing reading pd for writing child
				if (close_status == -1) { // close failed
					perror("Fd closing failed");
					exit(1);
				}
				arglist[pipe_index] = NULL; // first child only gets first command
				dup2_status = dup2(pipefds[1], 1); // make standard output of first child to be fd1
				if (dup2_status == -1) { // error in dup2
					perror("Dup2 failed");
					exit(1);
				}
				close_status = close(pipefds[1]); // closing writing pd because it was dupped
				if (close_status == -1) { // close failed
					perror("Fd closing failed");
					exit(1);
				}
				exec_status = execvp(arglist[0], arglist);
				if (exec_status == -1) { // exec failed
					perror("Execvp failed");
					exit(1);
				}
			}
			else { // parent
				int pid2 = fork();
				if (pid2 < 0) { // error in fork
					perror("Fork failed");
					exit(1);
				}
				if (pid2 == 0) { // second child executes the second command
					signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
					close_status = close(pipefds[1]); // closing writing pd for reading child
					if (close_status == -1) { // close failed
						perror("Fd closing failed");
						exit(1);
					}
					dup2_status = dup2(pipefds[0], 0); // make standard input of second child to be fd0
					if (dup2_status == -1) { // error in dup2
						perror("Dup2 failed");
						exit(1);
					}
					close_status = close(pipefds[0]); // closing reading pd because it was dupped
					if (close_status == -1) { // close failed
						perror("Fd closing failed");
						exit(1);
					}
					exec_status = execvp(arglist[pipe_index+1], &arglist[pipe_index+1]); // only sending the second command
					if (exec_status == -1) { // exec failed
						perror("Execvp failed");
						exit(1);
					}
				}
				else { // parent
					close_status = close(pipefds[0]);
					if (close_status == -1) { // close failed
						perror("Fd closing failed");
						exit(1);
					}
					close_status = close(pipefds[1]);
					if (close_status == -1) { // close failed
						perror("Fd closing failed");
						exit(1);
					}
					wait_finish_status = waitpid(pid1, &child_exit_status, 0); // waits for first child to finish
					if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
						perror("Waitpid failed");
						exit(1);
					}
					wait_finish_status = waitpid(pid2, &child_exit_status, 0); // parent waits for second child to finish
					if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
						perror("Waitpid failed");
						exit(1);
					}
				}
			}
		}
	}
	return 1;
}

int finalize(void)
{
	signal(SIGINT, SIG_DFL); // restore shell termination upon SIGINT
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