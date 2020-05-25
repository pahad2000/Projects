
// COMP1521 18s2 mysh ... command history
// Implements an abstract data object

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include "history.h"

// This is defined in string.h
// BUT ONLY if you use -std=gnu99
//extern char *strdup(const char *s);

// Command History
// array of command lines
// each is associated with a sequence number

#define MAXHIST 20
#define MAXSTR  200

#define HISTFILE ".mymysh_history"

typedef struct _history_entry {
   int   seqNumber;
   char *commandLine;
} HistoryEntry;

typedef struct _history_list {
   int nEntries;
   HistoryEntry commands[MAXHIST];
} HistoryList;

HistoryList CommandHistory;

// initCommandHistory()
// - initialise the data structure
// - read from .history if it exists

int initCommandHistory() {
   	CommandHistory.nEntries = 0;
   	int i = 0;
   	char *home = getenv("HOME");
   	char buffer[MAXSTR];
   	strcpy(buffer,home);
   	strcat(buffer,"/");
   	strcat(buffer,HISTFILE);
   	// Initialise the sequence numbers of each command entry and malloc space
   	// for the commandlines 
    for (i = 0; i < MAXHIST; i += 1) {
       CommandHistory.commands[i].commandLine = malloc(MAXSTR);
       CommandHistory.commands[i].seqNumber = i+1;
    }   	
   	i = 0;
    FILE *fp = fopen(buffer, "r");
    // Read in the data from .mymysh_history if it exists
    if (fp != NULL) {
       while (fscanf(fp, " %3d %[^\n]s", &CommandHistory.commands[i].seqNumber, CommandHistory.commands[i].commandLine) == 2) {
             CommandHistory.nEntries += 1;
             i += 1;
       }
	   fclose(fp);
    }
    // Return the largest (most recent) sequence number in CommandHistory
   	return CommandHistory.commands[19].seqNumber;
}

// addToCommandHistory()
// - add a command line to the history list
// - overwrite oldest entry if buffer is full

void addToCommandHistory(char *cmdLine, int seqNo) {
	int i = 0;
	// If the number of entries in CommandHistory is MAXHIST, then overwrite oldest entry in buffer
	if (CommandHistory.nEntries == MAXHIST) {
      // Make space for the newest command at the end of the commands array
	   for (i = 0; i < MAXHIST; i += 1) CommandHistory.commands[i].seqNumber += 1;
		
	   for (i = 0; i < MAXHIST-1; i += 1) {
		  strcpy(CommandHistory.commands[i].commandLine,CommandHistory.commands[i+1].commandLine);
	   }
		  strcpy(CommandHistory.commands[i].commandLine,cmdLine);
	} 
	// Otherwise, add a command line to the next free entry in CommandHistory 
	else {
	   while (CommandHistory.commands[i].seqNumber != seqNo) i += 1;
	   strcpy(CommandHistory.commands[i].commandLine, cmdLine);
	   CommandHistory.nEntries += 1;
    }
	return;
	 
}

// showCommandHistory()
// - displays the data in CommandHistory 
// - a sequence of commands indexed from 1 to 20 possibly containing a set of 
// commands entered by the user 

void showCommandHistory() {
	int i;
	for (i = 0; CommandHistory.commands[i].commandLine != NULL; i += 1) { 
	   printf(" %3d %s\n", CommandHistory.commands[i].seqNumber, CommandHistory.commands[i].commandLine);
    }
}

// getCommandFromHistory()
// - get the command line for specified command
// - returns NULL if no command with this number

char *getCommandFromHistory(int cmdNo) {
	// Iterate through the entries of CommandHistory and check whether  
    // the entry for cmdNo contains a command
	int i = 0;
	for (i = 0; i < CommandHistory.nEntries; i += 1) {
		if (CommandHistory.commands[i].seqNumber == cmdNo) {
			return CommandHistory.commands[i].commandLine;
		}
	}
	// Returns NULL if no command with cmdNo	
	return NULL; 
}

// saveCommandHistory()
// - write history to $HOME/.mymysh_history

void saveCommandHistory() {
    // Open .mymysh_history in the home directory 
	char *home = getenv("HOME");
   	char buffer[MAXSTR];
   	strcpy(buffer,home);
   	strcat(buffer,"/");
   	strcat(buffer,HISTFILE);
   	FILE *fd = fopen(buffer,"w");
    int i;
    // Write the contents of CommandHistory into .mymysh_history
	for (i = 0; i < CommandHistory.nEntries; i += 1) {
	    fprintf(fd," %3d  %s\n",CommandHistory.commands[i].seqNumber,CommandHistory.commands[i].commandLine);
	}
	fclose(fd);
}

// cleanCommandHistory
// - release all data allocated to command history

void cleanCommandHistory() {
    // Release the dynamically allocated command lines in CommandHistory
	int i = 0;
    for (i = 0; CommandHistory.commands[i].commandLine != NULL; i += 1) {
        free(CommandHistory.commands[i].commandLine);
    }
}
