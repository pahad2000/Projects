// mysh.c ... a small shell
// Started by John Shepherd, September 2018
// Completed by <Parviz Ahmadi>, September/October 2018

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include "history.h"


#define ISOUT 1
#define ISIN 2

// This is defined in string.h
// BUT ONLY if you use -std=gnu99
extern char *strdup(char *);

// Function forward references
void trim(char *);
int strContains(char *, char *);
char **tokenise(char *, char *);
char **fileNameExpand(char **);
void freeTokens(char **);
char *findExecutable(char *, char **);
int isExecutable(char *);
void prompt(void);
int commandlength(char **tokens); 



// Global Constants

#define MAXLINE 200

// Global Data

/* none ... unless you want some */

// Main program
// Set up enviroment and then run main loop
// - read command, execute command, repeat

int main(int argc, char *argv[], char *envp[])
{
   pid_t pid;   // pid of child process
   int stat;    // return status of child
   char **path; // array of directory names
   int cmdNo;   // command number
   int i;       // generic index
   char cwd[MAXLINE]; // current working directory 
   char line[MAXLINE]; // user input 
   char **tokenised_comm;
   char executable[MAXLINE];
   int fd[2];
   int instance[2];
   int initlen;
   int moresigns = 0;
   int found = 0;
   int redirectionvalid = 0;
   // set up command PATH from environment variable
   for (i = 0; envp[i] != NULL; i++) {
      if (strncmp(envp[i], "PATH=", 5) == 0) break;
   }
   if (envp[i] == NULL)
      path = tokenise("/bin:/usr/bin",":");
   else
      // &envp[i][5] skips over "PATH=" prefix
      path = tokenise(&envp[i][5],":");
#ifdef DBUG
   for (i = 0; path[i] != NULL;i++)
      printf("path[%d] = %s\n",i,path[i]);
#endif

   // initialise command history
   // - use content of ~/.mymysh_history file if it exists

   cmdNo = initCommandHistory();
   // main loop: print prompt, read line, execute command

   prompt();
   while (fgets(line, MAXLINE, stdin) != NULL) {
      trim(line); // remove leading/trailing space
      
      
      // if empty command, ignore
      if (!strcmp(line,"")) {
         prompt();
         continue;
      }
      
      // handle ! history substitution by first checking whether the first character of the user command is a !, and then checking if it is a !! command, or a valid !num command
      if (strchr(line,'!')-&line[0] == 0) {
           // Invoke most recently used command
         if (!strcmp(line,"!!")) {
              if (cmdNo != 0) {
                 strcpy(line,getCommandFromHistory(cmdNo));
                 cmdNo += 1;
                 addToCommandHistory(line,cmdNo);
              } 
              // If ./mymysh is being called for the first time, the command history is empty and no recently used command exists
              else {
                 printf("No command #%d\n", 0); 
                 prompt();
                 continue;
              }
          } else {
               char *seq_no = strchr(line,'!');
               int num;
               seq_no += 1; 
               // if the number following the ! is a valid sequence number then get the corresponding command from history 
               if (!(num = atoi(seq_no))) {
                  if (!strcmp(line,"!0")) { 
                        printf("No command #%d\n",0);
                        prompt();
                        continue;   
                  }
                  printf("Invalid History Substitution\n"); 
                  prompt();
                  continue;
               } 
                
               if (getCommandFromHistory(num) != NULL) {
                  strcpy(line,getCommandFromHistory(num));
               } else {
                  printf("No command #%d\n", num); 
                  prompt();
                  continue;
               }    
           }
       }
      
      // Tokenise the user command using whitespace delimiters 
      tokenised_comm = tokenise(line," ");
      // Keep track of the length of the original user command
      initlen = commandlength(tokenised_comm);
      // Handle *?[~ filename expansion
      tokenised_comm = fileNameExpand(tokenised_comm);
      // If exit command invoked, then break the loop
      if (!strcmp(tokenised_comm[0],"exit")) {
         cmdNo += 1;
         addToCommandHistory(line,cmdNo);
         break;
      } 
      
      // If h/history command invoked, then display the last 20 commands with their sequence numbers
      if (!strcmp(tokenised_comm[0],"h") || !strcmp(tokenised_comm[0],"history")) {
         showCommandHistory();  
         cmdNo += 1;
         addToCommandHistory(line,cmdNo);
         prompt();
         continue;
      }
      
      // If pwd command invoked, then print the current working directory 
      if (!strcmp(tokenised_comm[0],"pwd")) {
         if (getcwd(cwd,sizeof(cwd)) != NULL) {
            printf("%s\n",cwd);
            cmdNo += 1;
            addToCommandHistory(line,cmdNo);
            prompt();
            continue;
         } else {
            perror("Could not print current working directory\n");
            exit(1);
         }
      }
      // if cd command invoked, then change the working directory to the desired destination 
      if (!strcmp(tokenised_comm[0],"cd")) {
        // if the user command is "cd" only then change to the home directory 
         if (tokenised_comm[1] == NULL) {
            if (!chdir(getenv("HOME"))) {       
               printf("%s\n",getenv("HOME"));
               cmdNo += 1;
               addToCommandHistory(line,cmdNo);
            }
         } 
        // Otherwise move to the desired directory 
         else if (!chdir(tokenised_comm[1])) {
            getcwd(cwd,sizeof(cwd));
            printf("%s\n",cwd);
            cmdNo += 1;
            addToCommandHistory(line,cmdNo);
         } else if (chdir(tokenised_comm[1]) == -1) {
            printf("%s: No such file or directory\n",tokenised_comm[1]);
         }
         prompt();
         continue;
      }
 
      // check for input/output redirection
      for (i = 0; i < commandlength(tokenised_comm); i += 1) {
          // If the user input '>' as the redirection operator
          if (!strcmp(tokenised_comm[i],">") && !moresigns) {
             moresigns += 1;
             instance[0] = ISOUT;
             instance[1] = i + 1;
             // Check if the command has an output operator as its last token
             if (i == initlen-1) {
                printf("Invalid i/o redirection\n");
                break;
             } 
             // i/o redirection should not handle more than one file
             else if (i <= initlen - 3) { 
                printf("Invalid i/o redirection\n");
                break;
             } 
             // Check if the file that is to be outputted to exists, and if so, is writeable
             else if (open(tokenised_comm[i+1],O_WRONLY|O_CREAT) == -1) {
                printf("Output redirection: Permission denied\n");       
                break;         
             }
          } 
          // Having < as a token elsewhere in the command-line is an error
          else if (!strcmp(tokenised_comm[i],">") && moresigns) { 
            printf("Invalid i/o redirection\n");
            break;
          } 
          // If the user input '<' as the redirection operator
          else if (!strcmp(tokenised_comm[i],"<") && !moresigns) {
             moresigns += 1;
             instance[0] = ISIN;
             instance[1] = i+1;
             // Check if the command has an input operator as its last token
             if (i == initlen - 1) {
                printf("Invalid i/o redirection\n");
                break;
             } 
             // i/o redirection should not handle more than one file
             else if (i <= initlen - 3) { 
                printf("Invalid i/o redirection\n");
                break;
             } 
             // Check if the file taken as input is readable
             else if (open(tokenised_comm[i+1],O_RDONLY) < 0) {
                if (errno == EACCES) {
                   printf("Input redirection: Permission denied\n");
                   break;
                } else {
                   printf("Input redirection: No such file or directory\n");
                   break;
                }
             }
          } 
          // Having < as a token elsewhere in the command-line is an error
          else if (!strcmp(tokenised_comm[i],"<") && moresigns) {        
             printf("Invalid i/o redirection\n");
             break;
          } else if (!strcmp(tokenised_comm[i],"|")) {
             printf("Pipelines not implemented\n");
             prompt();
             break;
          }
          // If we have reached the last token of the user command then we can assume use of redirection is valid
          if (i == commandlength(tokenised_comm)-1) { 
              redirectionvalid = 1;
          }
      }
      
      // if the redirection is valid, signal whether a '>' or '<' operator has been used
      if (redirectionvalid) {
         moresigns = 0;
      } else if (!redirectionvalid) {
         moresigns = 0;
         instance[0] = 0;
         instance[1] = 0;
         prompt();
         continue;
      }
      
      // Find executable using first token of tokenised_comm
      if (findExecutable(tokenised_comm[0],path) == '\0') {
         printf("%s: Command not found\n", tokenised_comm[0]);
         prompt();
         continue;	        
      }
  
      found = 1;
      strcpy(executable,findExecutable(tokenised_comm[0],path));

      // Displays the absolute path of the executable found in a search of PATH
      printf("Running %s ...\n----------\n",executable);
      
      // Run the user command
      assert(pipe(fd) == 0);
      pid = fork();
      if (pid < 0) {
         perror("process unable to be duplicated\n");
         exit(1);
      } else if (pid > 0) {
         waitpid(pid,&stat,0);
         freeTokens(tokenised_comm);
         // if we reach the child process returns successfully, then the user comamnd is valid and thus can be added to the command history
         cmdNo += 1;
         addToCommandHistory(line,cmdNo);
         printf("----------\nReturns %d\n", WEXITSTATUS(stat));
      } else {
         if (found) {
            // Sort out any redirections
            if (instance[0] == ISIN) {
               tokenised_comm[instance[1]-1] = NULL;
               fd[0] = open(tokenised_comm[instance[1]],O_RDONLY);
               dup2(fd[0],0);
               close(fd[0]);
            } else if (instance[0] == ISOUT) {
               tokenised_comm[instance[1]-1] = NULL;
               fd[1] = open(tokenised_comm[instance[1]],O_WRONLY|O_CREAT);
               dup2(fd[1],1);
               close(fd[1]);
            }
            // If valid redirection operators present 
            // Run the command
            execve(executable, tokenised_comm, envp);
         }
          
        
      }
      redirectionvalid = 0;
      instance[0] = 0;
      instance[1] = 0;
      found = 0;

            
      prompt();
   }
   saveCommandHistory();
   cleanCommandHistory();
   printf("\n");
   return(EXIT_SUCCESS);
}

// commandlength: calculate the length of a token
int commandlength(char **tokens) {
   int i;
   for (i = 0; tokens[i] != NULL; i += 1);
   return i;
}
// fileNameExpand: expand any wildcards in command-line args
// - returns a possibly larger set of tokens
char **fileNameExpand(char **tokens) {
   // Obtain preliminary data about the number of wildcards in command-line args
   glob_t globbuf;
   int i = 0;
   int j = 0;
   int k = 0;
   int slots_needed = 0;
   
   // Iterate through each token in the user command to determine the number of
   // additional slots required for wildcard translation
   for (i = 0; i < commandlength(tokens); i += 1) {
       if (strContains(tokens[i],"*")) {
          if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
             slots_needed += globbuf.gl_pathc-1;
          }
       } else if (strContains(tokens[i],"[")) {
          if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
             slots_needed += globbuf.gl_pathc-1;
          }
       } else if (strContains(tokens[i], "?")) {
          if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
             slots_needed += globbuf.gl_pathc-1;
          }
       } else if (strContains(tokens[i], "~")) {
          if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
             slots_needed += globbuf.gl_pathc-1;
          }
       } else { 
          // If no additional slots found, then no need to malloc or free memory as per the glob() function
          continue;
       }
          globfree(&globbuf);
   }
   // Add the length of the original token to the number of slots needed
   slots_needed += commandlength(tokens) + 1;
   // Malloc a corresponding amount of memory for the new token
   char **new = malloc(sizeof(char*)*slots_needed);
   for (i = 0; i < slots_needed; i += 1) new[i] = malloc(MAXLINE);
   
   // Implement the wildcard translation by intialising a new set of tokens 
   // that includes the additional number of slots as well as the original
   // user command
   for (i = 0; i < commandlength(tokens); i += 1) {
       if (strContains(tokens[i],"*")) {
          if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
             for (j = 0; j < globbuf.gl_pathc; j += 1) {
                 strcpy(new[k],globbuf.gl_pathv[j]);
                 k += 1;
             }
          }
       } else if (strContains(tokens[i],"[")) {
           if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
               for (j = 0; j < globbuf.gl_pathc; j += 1) {
                   strcpy(new[k],globbuf.gl_pathv[j]);
                   k += 1;
               }
           }
       } else if (strContains(tokens[i],"?")) {
           if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
               for (j = 0; j < globbuf.gl_pathc; j += 1) {
                   strcpy(new[k],globbuf.gl_pathv[j]);
                   k += 1;
               }
           }
       } else if (strContains(tokens[i],"~")) {
           if (!glob(tokens[i],GLOB_NOCHECK|GLOB_TILDE,NULL,&globbuf)) {
               for (j = 0; j < globbuf.gl_pathc; j += 1) {
                   strcpy(new[k],globbuf.gl_pathv[j]);
                   k += 1;
               }
           }
       } else {
           strcpy(new[k],tokens[i]);
           k += 1;
           continue;
       }     
       globfree(&globbuf);
   }
   // Null terminate the string
   new[slots_needed-1] = NULL;
   // Return a possibly larger set of tokens 
   free(tokens);
   return new;
}

// findExecutable: look for executable in PATH
char *findExecutable(char *cmd, char **path) {
    char executable[MAXLINE];
    executable[0] = '\0';
    if (cmd[0] == '/' || cmd[0] == '.') {
       strcpy(executable, cmd);
       if (!isExecutable(executable))
          executable[0] = '\0';
    }
    else {
       int i;
       for (i = 0; path[i] != NULL; i++) {
          sprintf(executable, "%s/%s", path[i], cmd);
          if (isExecutable(executable)) break;
       }
       if (path[i] == NULL) executable[0] = '\0';
    }
    if (executable[0] == '\0')
       return NULL;
    else
       return strdup(executable);
}

// isExecutable: check whether this process can execute a file
int isExecutable(char *cmd) {
   struct stat s;
   // must be accessible
   if (stat(cmd, &s) < 0)
      return 0;
   // must be a regular file
   //if (!(s.st_mode & S_IFREG))
   if (!S_ISREG(s.st_mode))
      return 0;
   // if it's owner executable by us, ok
   if (s.st_uid == getuid() && s.st_mode & S_IXUSR)
      return 1;
   // if it's group executable by us, ok
   if (s.st_gid == getgid() && s.st_mode & S_IXGRP)
      return 1;
   // if it's other executable by us, ok
   if (s.st_mode & S_IXOTH)
      return 1;
   return 0;
}

// tokenise: split a string around a set of separators
// create an array of separate strings
// final array element contains NULL
char **tokenise(char *str, char *sep) {
   // temp copy of string, because strtok() mangles it
   char *tmp;
   // count tokens
   tmp = strdup(str);
   int n = 0;
   strtok(tmp, sep); n++;
   while (strtok(NULL, sep) != NULL) n++;
   free(tmp);
   // allocate array for argv strings
   char **strings = malloc((n+1)*sizeof(char *));
   assert(strings != NULL);
   // now tokenise and fill array
   tmp = strdup(str);
   char *next; int i = 0;
   next = strtok(tmp, sep);
   strings[i++] = strdup(next);
   while ((next = strtok(NULL,sep)) != NULL)
      strings[i++] = strdup(next);
   strings[i] = NULL;
   free(tmp);
   return strings;
}

// freeTokens: free memory associated with array of tokens
void freeTokens(char **toks) {
   for (int i = 0; toks[i] != NULL; i++)
      free(toks[i]);
   free(toks);
}

// trim: remove leading/trailing spaces from a string
void trim(char *str) {
   int first, last;
   first = 0;
   while (isspace(str[first])) first++;
   last  = strlen(str)-1;
   while (isspace(str[last])) last--;
   int i, j = 0;
   for (i = first; i <= last; i++) str[j++] = str[i];
   str[j] = '\0';
}

// strContains: does the first string contain any char from 2nd string?
int strContains(char *str, char *chars) {
   for (char *s = str; *s != '\0'; s++) {
      for (char *c = chars; *c != '\0'; c++) {
         if (*s == *c) return 1;
      }
   }
   return 0;
}

// prompt: print a shell prompt
// done as a function to allow switching to $PS1
void prompt(void) {
   printf("mymysh$ ");
}



