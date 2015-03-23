
#ifndef COMMAND_H
#define	COMMAND_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct command
{
    char* args[50];
    int numargs;
};

typedef struct command* commandPTR;

struct localCommands
{
	char *name;
	int (*function)(int argc, char** argv);
};

int myCd(int, char**);
int myExit(int, char**);


#endif
