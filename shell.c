#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>

/*Group Members:

Zachary Scott */


char* tokens[1024];
commandPTR commands[1024];


int tokenindex = 0;
int commandindex = 0;
int activeCommand = 0;
struct localCommands locals[2]= {{"exit",myExit},{"cd",myCd}};
int argflag = 0;
int quoteflag = 0;

int parseInput (char* buffer)
{

    if (!buffer)
    {
        printf("No input, error \n");
        return 0;
    }
    
    int i = 0;
    char QuoteType;
    int QuoteMatch = 0;
    int pipeflag = 0;
    int tokensize = 0;

    int length = strlen(buffer);

    char* token = buffer;

    if ( length > 0)
    {
        buffer[length-1] = 0;   // clear newline character
    }    

    while ((*buffer) != 0)
    {    

        if ( (*buffer) == '\"' || (*buffer) == '\'') // token is quoted
        {
            QuoteType = *buffer;
            token = ++buffer;

            while ((*buffer != 0) && ( (*buffer) != QuoteType))
            {
                //printf("searching for %c : %c \n", QuoteType, *buffer );
                tokensize++;
                ++buffer;
            }
            if ( (*buffer) == 0) // reached end of input without matching quote 
            {
                printf("Quote mismatch, bad input \n");
                quoteflag = 1;
                return 0;
            }
            else // found matching quote! tokenize it
            {
                *buffer++ = '\0';
                tokens[tokenindex] = token;
                tokenindex++;
                tokensize = 0;
            }
        }
        else
        {
            while ( (*buffer) != 0 &&  !isspace(*buffer) && (*buffer) != '|')
            {
                tokensize++;
                ++buffer;
            }
            if ( (*buffer) == '|')
            {
                pipeflag = 1;
            }
            if (!tokensize) // sitting on a space or pipe with no token before it, move it along
            {
                ++buffer;
            }
            if (tokensize != 0) // dont want spaces or 0's tokenized
            {    
            *buffer++ = '\0';
            tokens[tokenindex] = token;
            tokenindex++;
            tokensize = 0;
            }
            if (pipeflag)
            {
                tokens[tokenindex] = "PIPE";
                tokenindex++;
                pipeflag = 0;
            } 
        }
        token = buffer;   // move token pointer to new position in buffer
    } 
    return 1; 
 }

 commandPTR createCommand()
 {
    commandPTR newCommand = (commandPTR) malloc(sizeof(struct command));
    newCommand->numargs = 0;
    return newCommand;
 }

 int buildCommands()
 {
    int i = 0;
    int j = 0;
    commandPTR currentCommand;
    commandPTR newCommand = createCommand();
    commands[commandindex] = newCommand;
    for (i = 0; i<tokenindex; i++)
    {
        currentCommand = commands[commandindex];

        if ( strcmp(tokens[i], "PIPE") != 0) // normal arg
        {
            currentCommand->args[j] = tokens[i];
            currentCommand->numargs++;
            j++;
        }
        else if ( strcmp(tokens[i], "PIPE") == 0) // if we get a pipe, make a new command
        {
            commandPTR newCommand = createCommand();
            commandindex++;
            commands[commandindex] = newCommand;
            j = 0;                                             // reset struct internal arg index
        }

        if (j==50)
        {
            printf("error: too many arguments \n");
            argflag = 1;
            return -1;
        }
    }
 }

 int pid;

 void runsource(int pfd[], char** argv, int pipes)
{
    int x;
    switch(pid = fork())
    {
        case 0:
            dup2(pfd[1], 1);              // set up output for first command
            for(x = 0; x < pipes; ++x)
            {
                close(pfd[2 * x]);          // make sure others are closed so it doesn't get confused
                close(pfd[2 * x + 1]);
            }   
            execvp(argv[0], argv);    // run command (loacted at index 0)
            perror(argv[0]);           // check for errors
        default:
            break;
        case -1:
            perror("fork");
            exit(1);
    }
}


void rundest(int pfd[], char **argv, int pipes)
{
    int x;
    switch(pid = fork())
    {
        case 0:
            dup2(pfd[2 * (pipes - 1)], 0);   // set the input on the last command
            for(x = 0; x < pipes; ++x)
            {
                close(pfd[2 * x]);         // close all the previous pairs
                close(pfd[2 * x + 1]);
            }
            
            execvp(argv[0], argv); 
            perror(argv[0]);
        default:
            break;
        case -1:
            perror("fork");
            exit(1);
    }
}

void runmiddle(int pfd[], char **argv, int pipes, int i)
{
    int x;
    switch(pid = fork())
    {
        case 0:
            dup2(pfd[2 * i], 0);                 // set up input channels
            dup2(pfd[2 * (i + 1) + 1], 1);     // set up output channels
            for(x = 0; x < pipes; ++x)
            {
                close(pfd[2 * x]);
                close(pfd[2 * x + 1]);
            }
            
            execvp(argv[0], argv);
            perror(argv[0]);
        default:
            break;
        case -1:
            perror("fork");
            exit(1);
    }
}

int simpleCommand(int argc, char** argv) 
{
    int status;
    switch(pid = fork())
    {
        case 0:
        {
            int response = execvp(argv[0], argv); // run the command 
            if (response == -1) 
            {
                printf("error: invalid command \n");
                exit(0);
            }               
            break;
        }       
        default:               // parent (shell) waits for the command to finish
        {
            while((pid = wait(&status)) != -1)
            fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
            break;
        }       
        case -1:
        {
            perror("fork");
            exit(1);
        }
    }
    return 0;
}

int runCommands()
{
    int numCommands = 0;
    commandPTR currentCommand = commands[0];
    if(commands[0] == 0) 
    {
        perror("no commands");
    }
    if(strcmp(currentCommand->args[0], locals[0].name) == 0)             // exit
    {
        locals[0].function( (currentCommand->numargs-1), currentCommand->args );   // first arg is command name so sub 1
    }
    else if(strcmp(currentCommand->args[0], locals[1].name) == 0)        // cd
    {
        locals[1].function( (currentCommand->numargs -1), currentCommand->args );  
    }

    else
    {
        int i;
        for (i=0; commands[i] != 0; i++)
        {
            numCommands++;                // figure out how many commands we have 
        }
        if( numCommands == 1 ) /* simpleCommand*/
        {
            simpleCommand(currentCommand->numargs, currentCommand->args);
        }
        else   
        {
            int status;
            int pipes = numCommands-1; // num pipes are total commands minus 1
            int fd[2 * pipes];
            int j;
            for(j = 0; j < pipes; ++j) // set up input and output for every pair of elements in the fd array
            {
                pipe(fd + 2 * j);
            }
            runsource(fd, currentCommand->args, pipes);    // run first command (only need to setup output)
            activeCommand++;

            for(j = 0; j < (pipes - 1); ++j)                  
            {
                currentCommand = commands[activeCommand];
                runmiddle(fd, currentCommand->args, pipes, j);  // run middle commands (set up both input and output)
                activeCommand++;
            }
            rundest(fd, currentCommand->args, pipes);           // set up last command (only set up input)
            for(j = 0; j < pipes; ++j)
            {
                close(fd[2 * j]);                     // close all the pairs (odds and evens)
                close(fd[2 * j + 1]);
            }
            while((pid = wait(&status)) != -1)           // wait for children to exit
            {
                fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
            }
        }
    }    
    return 0;
}

int myCd(int argc, char** argv)
{
    char* pathname = NULL;
    char* currentPath =NULL;
    char* desiredDIR = NULL;
    int response = 0;

    if(argc == 0)  // no arguments, go to home dir
    {
        pathname = getenv("HOME"); /* return a null-terminated string with the value of the requested environment variable */
        response = chdir(pathname);  // hopefully n becomes 0 if we got to home director
     }
    else if(argc == 1)
    {
        desiredDIR = argv[1];
        currentPath = getcwd(currentPath,100);           // absolute pathname of current DIR
        pathname = strcat(currentPath,"/");            // add a slash
        pathname = strcat(pathname,desiredDIR);           // add the desired director
        response = chdir(pathname);                        // try to change it, should return 0
        
        if(response == -1)
        {              
            printf("error: could not change directory to : %s \n", pathname);
            return -1;
        }                 
    }
    else
    {
        fprintf(stderr, "error: too many arguments. (expected 0..1)\n");
        return -1;
    }
    return 0;
}

int myExit(int argc, char** argv)
{ 
    int exitcode = 0;

    if(argc == 0)     // no arguments given
    {
        exitcode = 0;
    }
    else if (argc == 1)
    {
        exitcode = atoi(argv[1]);
       // printf("exit code %d \n", exitcode);   
    }
    else if (argc > 1)
    {
        fprintf(stderr, "too many arguments (expected: 0..1) \n");   
        exitcode = -1;  
    }
    _exit(exitcode);
}

void CleanUp(void)
{
    tokenindex = 0;
    commandindex = 0;
    activeCommand = 0;
    memset(commands, 0, sizeof(commands));
    memset(tokens, 0, sizeof(tokens));
    argflag = 0;
    quoteflag = 0;
}


int main()
{
    int human = isatty(0);
    while(1)
    {
        int i =0;
        int j = 0;
        commandPTR currentCommand;
        char buffer[1024];
        if(human)
        {
            printf("(ツ) ");          
            fgets(buffer, 1024, stdin);   
        }
        else                   // not from terminal
        {
            fgets(buffer, 1024, stdin);
        }
        if(feof(stdin))
        {
            exit(0);
        }
		parseInput(buffer);   
        buildCommands();

        if (tokens[0] != NULL && argflag == 0 && quoteflag == 0) // user put in nothing or they put in too many commands
        {    
            runCommands();
        }
        CleanUp();   
    }
}