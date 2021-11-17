#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

void exit(int status);

void sigchld_handler(int signum, siginfo_t* info, void *ptr) { // handler for deleting zombies
	int pid = info->si_pid; // pid of child that raised SIGCHLD
	int wait_finish_status = waitpid(pid, NULL, 0); // parent waits for child to finish
	if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
		perror("Waitpid in SIGCHLD handler failed");
		exit(1);
	}
}

int prepare(void)
{
	signal(SIGINT, SIG_IGN); // shell should not terminate upon SIGINT
	struct sigaction sigchld_action; // struct of sigaction to pass to registration
	memset(&sigchld_action, 0, sizeof(sigchld_action)); // setting sigaction mem to 0
	sigchld_action.sa_sigaction = sigchld_handler; // setting handler to my function
	sigchld_action.sa_flags = SA_RESTART; // reseting flags
	if (sigaction(SIGCHLD, &sigchld_action, NULL) != 0) { // registering handler
		perror("Error in SIGCHLD handler registration");
		return 1;
	}
	return 0;
}

int run_background_process(int count, char **arglist) {
	int pid = fork();
	if (pid < 0) { // error in fork
		perror("Fork failed");
		return 0;
	}
	if (pid == 0) { // child executes the command
		arglist[count-1] = NULL; // not send "&" symbol to execvp
		int exec_status = execvp(arglist[0], arglist);
		if (exec_status == -1) { // exec failed
			perror("Execvp failed");
			exit(1);
		}
	}
	// parent does not wait for child to finish
	return 1;
}

int run_redirection(int count, char **arglist) {
	char *filename = arglist[count-1];
	int fd = open(filename, O_RDWR | O_CREAT, 00777);
	if (fd < 0) { // open failed
		perror("Open or create file failed");
		return 0;
	}
	int pid = fork();
	if (pid < 0) { // error in fork
		perror("Fork failed");
		return 0;
	}
	if (pid == 0) { // child executes the command
		signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
		arglist[count-2] = NULL; // not send ">" symbol and filename to execvp
		int dup2_status = dup2(fd, 1); // make standard output of child to be file
		if (dup2_status == -1) { // error in dup2
			perror("Dup2 failed");
			exit(1);
		}
		int close_status = close(fd); // closing file
		if (close_status == -1) { // close failed
			perror("File closing failed");
			exit(1);
		}
		int exec_status = execvp(arglist[0], arglist);
		if (exec_status == -1) { // exec failed
			perror("Execvp failed");
			exit(1);
		}
	}
	else { // parent waits for child to finish
		int wait_finish_status = waitpid(pid, NULL, 0);
		if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
			perror("Waitpid failed");
			return 0;
		}
		int close_status = close(fd); // closing file
		if (close_status == -1) { // close failed
			perror("File closing failed");
			return 0;
		}
	}
	return 1;
}

int run_regular_cmd(int count, char **arglist) {
	int pid = fork();
	if (pid < 0) { // error in fork
		perror("Fork failed");
		return 0;
	}
	if (pid == 0) { // child executes the command
		signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
		int exec_status = execvp(arglist[0], arglist);
		if (exec_status == -1) { // exec failed
			perror("Execvp failed");
			exit(1);
		}
	}
	else { // parent waits for child to finish
		int wait_finish_status = waitpid(pid, NULL, 0);
		if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
			perror("Waitpid failed");
			return 0;
		}
	}
	return 1;
}

int get_command_pipe_index(int count, char **arglist) {
	for (int i = 0; i < count; i++) {
		if (strcmp(arglist[i], "|") == 0) {
			return i;
		}
	}
	return -1;
}

int run_piped_commands(int count, char **arglist, int pipe_index) {
	int pipefds[2]; // array for fds from pipe 
	int pipe_status = pipe(pipefds);
	if (pipe_status == -1) {
		perror("Pipe creation failed");
		return 0;
	}
	int pid1 = fork();
	if (pid1 < 0) { // error in fork
		perror("Fork failed");
		return 0;
	}
	if (pid1 == 0) { // first child executes the first command
		signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
		int close_status = close(pipefds[0]); // closing reading pd for writing child
		if (close_status == -1) { // close failed
			perror("Fd closing failed");
			exit(1);
		}
		arglist[pipe_index] = NULL; // first child only gets first command
		int dup2_status = dup2(pipefds[1], 1); // make standard output of first child to be fd1
		if (dup2_status == -1) { // error in dup2
			perror("Dup2 failed");
			exit(1);
		}
		close_status = close(pipefds[1]); // closing writing pd because it was dupped
		if (close_status == -1) { // close failed
			perror("Fd closing failed");
			exit(1);
		}
		int exec_status = execvp(arglist[0], arglist);
		if (exec_status == -1) { // exec failed
			perror("Execvp failed");
			exit(1);
		}
	}
	else { // parent
		int pid2 = fork();
		if (pid2 < 0) { // error in fork
			perror("Fork failed");
			return 0;
		}
		if (pid2 == 0) { // second child executes the second command
			signal(SIGINT, SIG_DFL); // foreground child should terminate upon SIGINT
			int close_status = close(pipefds[1]); // closing writing pd for reading child
			if (close_status == -1) { // close failed
				perror("Fd closing failed");
				exit(1);
			}
			int dup2_status = dup2(pipefds[0], 0); // make standard input of second child to be fd0
			if (dup2_status == -1) { // error in dup2
				perror("Dup2 failed");
				exit(1);
			}
			close_status = close(pipefds[0]); // closing reading pd because it was dupped
			if (close_status == -1) { // close failed
				perror("Fd closing failed");
				exit(1);
			}
			int exec_status = execvp(arglist[pipe_index+1], &arglist[pipe_index+1]); // only sending the second command
			if (exec_status == -1) { // exec failed
				perror("Execvp failed");
				exit(1);
			}
		}
		else { // parent
			int close_status = close(pipefds[0]);
			if (close_status == -1) { // close failed
				perror("Fd closing failed");
				return 0;
			}
			close_status = close(pipefds[1]);
			if (close_status == -1) { // close failed
				perror("Fd closing failed");
				return 0;
			}
			int wait_finish_status = waitpid(pid1, NULL, 0); // waits for first child to finish
			if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
				perror("Waitpid failed");
				return 0;
			}
			wait_finish_status = waitpid(pid2, NULL, 0); // parent waits for second child to finish
			if (wait_finish_status == -1 && errno != ECHILD && errno != EINTR) { // real error in waiting
				perror("Waitpid failed");
				return 0;
			}
		}
	}
	return 1;
}

int process_arglist(int count, char **arglist) {
	if (strcmp(arglist[count-1], "&") == 0) { // process needs to run in the background
		if (run_background_process(count, arglist) == 0) {
			return 0;
		}
	}

	else if (count > 1 && strcmp(arglist[count-2], ">") == 0) { // command has output redirecting
		if (run_redirection(count, arglist) == 0) {
			return 0;
		}
	}

	else {
		int pipe_index = get_command_pipe_index(count, arglist);
		if (pipe_index == -1) { // regular single command 
			if (run_regular_cmd(count, arglist) == 0) {
				return 0;
			}
		}
		else { // command has a pipe
			if (run_piped_commands(count, arglist, pipe_index) == 0) {
				return 0;
			}
		}
	}
	return 1;
}

int finalize(void)
{
	signal(SIGINT, SIG_DFL); // restore shell termination upon SIGINT
	signal(SIGCHLD, SIG_DFL); // restore shell behavior upon SIGCHLD
	return 0;
}