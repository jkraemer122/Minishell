#define _GNU_SOURCE
#define DELIMITERS " \n" // space and newlines are delimiters
#define STACK_SIZE (1024 * 1024) 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>

void shell_loop();
int makeargv(const char *s, const char *delimiters, char ***argvp);
int forkEx(char** argvp, int waitProc); 
int cloneInterface(int interface);

int main(int argc, char *argv[])
{
  printf("You have entered the mini shell. \n");
  shell_loop();
  return 0;
}


void shell_loop()
{
  int run = 1;
  while(run)
  {
    printf("--> $ ");

    char** argvp;
    char *input = NULL;
    size_t buf_len = 0;
    ssize_t numchars = getline(&input, &buf_len, stdin);

    if(numchars == -1)
    {
      fprintf(stderr, "Input read failed. \n");
    }

    int argvSize = makeargv(input, DELIMITERS, &argvp);

    // stops Segfault on enter
    if(argvSize < 1)
    {
      free(input);
      free(argvp);
      continue; // restart
    }

    //  Process Control 
    if(strcmp(argvp[0],"exit") == 0 || strcmp(argvp[0],"quit") == 0)
    {
      run = 0;
      continue;
    }
      else if (strcmp(argvp[0], "clone") == 0)
      {
        if (argvSize == 1)
        {
          printf("Please specify additional parameters. Try: \n\t clone files\n\t clone fs \n\t clone io\n\t clone net\n\t clone ns\n\t clone vm\n");
          free(input);
          free(argvp);
          continue;
        }
        else if (strcmp(argvp[1], "net") == 0)
          cloneInterface(CLONE_NEWNET);
        else if (strcmp(argvp[1], "ns") == 0)
          cloneInterface(CLONE_NEWNS);
        else if (strcmp(argvp[1], "files") == 0)
          cloneInterface(CLONE_FILES);
        else if (strcmp(argvp[1], "fs") == 0)
          cloneInterface(CLONE_FS);
        else if (strcmp(argvp[1], "io") == 0)
          cloneInterface(CLONE_IO);
        else if (strcmp(argvp[1], "vm") == 0)
          cloneInterface(CLONE_VM);
        else
        {
          printf("Not recognized. Type 'clone' for help. \n");
          free(input);
          free(argvp);
          continue;
        }
      }
    else if(strcmp(argvp[argvSize-1],"&") == 0)
    {
      argvp[argvSize-1] = '\0';
      // no wait for bg process
      forkEx(argvp, 0);
    }
    else
    {
      forkEx(argvp, 1);
    }
      
    free(input);
    free(argvp);
  }
}

// Credit: Unix Systems Programming Book, Program 2.2 -- makeargv.c
int makeargv(const char *s, const char *delimiters, char ***argvp)
{
  int error;
  int i;
  int numTokens;
  const char *snew;
  char *t;

  if ((s == NULL) || (delimiters == NULL) || (argvp == NULL))
  {
    errno = EINVAL;
    return -1;
  }

  *argvp = NULL; //pointer to array of strings initialized to NULL

  /* Pointer arithmethic making snew point to the real start of the 
   * string due to delimiters.
   */
  snew = s + strspn(s, delimiters); 

  // allocate t memory 
  if ((t = malloc(strlen(snew) + 1)) == NULL) // +1 for null term
    return -1;

  strcpy(t, snew); //strtok does in place tokenization
  numTokens = 0;
  if (strtok(t, delimiters) != NULL)
    // count number of tokens 
    for (numTokens = 1; strtok(NULL, delimiters) != NULL; numTokens++);

  // allocate memory for array of pointers to tokens
  if (( *argvp = malloc((numTokens + 1) * sizeof(char *))) == NULL)
  {
    error = errno;
    free(t);
    errno = error;
    return -1;
  }

  if (numTokens == 0)
    free(t);
  else 
  {
    strcpy(t, snew);
    **argvp = strtok(t, delimiters);
    for(i = 1; i < numTokens; i++)
      *( (*argvp) + i) = strtok(NULL, delimiters); 
  }
  
  *( (*argvp) + numTokens) = NULL; //put null pointer at end of argvp array

  return numTokens;
}

/*
 * Helper function that received a proper argument array
 * and passes it to forkEx. 
 * waitpid if and only if told to waitProc (1 or 0)
*/

int forkEx(char** argvp, int waitProc)
{
  pid_t pid;
  pid = fork();
  int status = 0;
  if (pid == 0) //child process
  {
    //printf("CHILD process: %d \n", pid);
    if(execvp(argvp[0], argvp) < 0)
    {
	 fprintf(stderr,"Error, no command '%s' found or incorrect arguments.\n", argvp[0]);
	 exit(EXIT_FAILURE);
    } 
    _exit(0);
    //exit call is reached unless execvp returns with an error
    //execvp will replace the current running proc with the called proc
  }
  else if (pid > 0) //parent process
  {
    //printf("PARENT process: %d \n", pid);
    if(waitProc)
    {
      while(waitpid(pid, &status, 0) < 0)
      {
	if(errno != EINTR)
	{
	  status = -1;
	  break;
	}
      }
    }
  }
  else //error
  {
    fprintf(stderr, "Error Forking Process. \n");
    status = -1;
  }
  return status;
}

/* ---- Child Functions passed to clone calls in Clone Interface ----*/
int fn_SpawnShell()
{
     printf("\n\t --- Spawning new BASH Subshell with PID: %ld ---\n", (long)getpid());
     system("/bin/bash");
     // unshare(CLONE_NEWNS); //this has the same effect as CLONE_NEWNS
     return 0;
}

int fn_testNetwork() 
{
     printf("\n\t --- NEW Cloned Network is PID: %ld ---\n", (long)getpid());
     system("ip link");
     return fn_SpawnShell();
}



int cloneInterface(int interface)
{
     char* stack = malloc(STACK_SIZE);
     char* stackTop = stack + STACK_SIZE;
     pid_t child_pid;

     if(interface == CLONE_NEWNS)
     {
	  printf("\t --- Copying namespace, original PID: %ld --- \n", (long)getpid());
	  child_pid = clone(fn_SpawnShell, stackTop, interface | CLONE_NEWNS | SIGCHLD, NULL);
     }
     else if(interface == CLONE_IO)
     {
	  printf("\t --- Sharing IO namespaces. \n");
	  child_pid = clone(fn_SpawnShell, stackTop, interface | CLONE_IO | SIGCHLD, NULL);
     }
     else if(interface == CLONE_FILES)
     {
	  printf("\t --- Sharing File Descriptor Table --- \n");
	  child_pid = clone(fn_SpawnShell, stackTop, interface | CLONE_FILES | SIGCHLD, NULL);
     }
     else if(interface == CLONE_FS)
     {
	  printf("\t --- Sharing Filesystem --- \n");
	  child_pid = clone(fn_SpawnShell, stackTop, interface | CLONE_FS | SIGCHLD, NULL);
     }
     else if(interface == CLONE_VM)
     {
	  printf("\t --- Running Child Process in Parent Memory --- \n");
	  child_pid = clone(fn_SpawnShell, stackTop, interface | CLONE_VM | SIGCHLD, NULL);
     }
     else if(interface == CLONE_NEWNET)
     {
	  printf("\t --- ORIGINAL Network is PID: %ld  --- \n", (long)getpid());
	  system("ip link");

	  child_pid = clone(fn_testNetwork, stackTop, interface | SIGCHLD, NULL);
	  //note:  CLONE_NEWPID flag will break child away from parent and not return execution to parent
     }
     else //shouldn't happen
     {
	  printf("Invalid/unused CLONE flags. \n");
	  return -1;
     }


     if(child_pid == -1)
	  printf("\nERROR: Failed to clone interface. Are you running shell as root/sudo? \n");
     else //wait for child.
     {
	  waitpid(child_pid, NULL, 0);
     }
     return child_pid;
}




int getInputWithoutDelimiter(char** input)
{
  size_t len = 0;
  ssize_t numchars = getline(input, &len, stdin);
  
  if(numchars == -1)
  {
    fprintf(stderr, "Failed to read input. \n");
    return -1;
  }

  if(*(*input + numchars -1)  == '\n')
  {
    *(*input + numchars -1) = '\0'; //null terminator
    --numchars;
  }

  return numchars;
}
