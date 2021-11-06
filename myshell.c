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
	if (strcmp(arglist[count], "&") == 0) { // process needs to run in the background
		// TODO
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
			// TODO
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
