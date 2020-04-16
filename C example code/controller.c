/**************************************************************
	controller.c
	Implementation for the controller process
***************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "controller.h"
#include <errno.h>
#include <stddef.h>
#include <sys/wait.h>
#include <stdbool.h>

void do_initialize(int pipes[][NUMPIPES], pid_t idList[]);
void do_child(int respondingPipe[]);
void readyHandler(int sig);
void setupHandler();
void do_game(int numRounds, int pipeList[][NUMPIPES], pid_t idList[]);
int evaluate(char player0, char player1);
void printResults(int wins[]);

int main(int argc, char** argv){
	int numRounds, i, err;
	pid_t childId[NUMPLAYERS];
	int pipeList[NUMPLAYERS][NUM_PIPE_ENDS];
	
	if(argc != 2){ //Required to have number of rounds as a parameter
		printf("Error, invalid number of arguments. Required: 1\n");
		exit(1);
	}//end if
	numRounds = atoi(argv[1]);
	
	do_initialize(pipeList, childId);

	/* Wait for both players to signal "Ready" before continuing */
	for(i = 0; i < NUMPLAYERS; i++)
		pause();
	/* Once both are ready, continue looping until enough games have been played. */
	do_game(numRounds, pipeList, childId);
	
	printf("Controller: terminating\n");
	err = sleep(1); //Prevent formatting issues in terminal
	if(err < 0){
		printf("Controller: Error in sleep - %d\n", errno);
		exit(21);
	}//end if
	return 0;
}//end main



/*********************************************************************************
do_initialize
Called when the controller is started. Performs initialization tasks, including:
	* Setting up the signal handler so that player can send controller signals
	* Creating two pipes to be used
	* Forking twice to create two child processes
	* Records the IDs of the children in the passed array.
	* Sending children to their respective function
	* Parent will close its necessary pipe endings
	* Parent returns normally
*********************************************************************************/
void do_initialize(int pipes[][NUMPIPES], pid_t idList[]){
	int err, i;
	/**********************************************************************
	Setting up the signal handler so that we can receive signals.
	**********************************************************************/
	setupHandler();
	/* Creating the pipes: */
	for(i = 0; i < NUMPIPES; i++){
		err = pipe(pipes[i]);
		if(err < 0){
			printf("Error in creating pipe%d; Terminating.\n", i);
			exit(2);
		}//end if
	}//end for(i)
	
	/* Creating the child processes: */
	for(i = 0; i < NUMPLAYERS; i++){
		idList[i] = fork();
		if(idList[i] < 0){
			printf("Error in forking child %d, terminating\n", i);
			exit(3);
		}//end if
		if(idList[i] == 0) //Then in child
			do_child(pipes[i]);
	}//end for(i)
	
	//Both children have forked off at this point.
	/* Close endings for each player's pipe: */
	for(i = 0; i < NUMPLAYERS; i++)
		close(pipes[i][WRITE]);
	
}//end do_initialize


/*********************************************************************************
do_child
When do_initialize forks a child process, that process is sent here.
Does tasks that prepare to exec the player process, including:
	* Closing necessary pipe endings
	* Casts the pipes to pass them through execve
	* Prepares the parameter array
	* Executes the player process
**********************************************************************************/
void do_child(int respondingPipe[]){
	int err;
	char* paramVector[4];
	char resPipeBuffer[11];
	
	/* Closing the necessary endings of the pipes */
	err = close(respondingPipe[READ]);
	if(err < 0){
		printf("Controller: Error in closing read end of pipe in child: %d\n", errno);
		exit(18);
	}//end if
	
	/* Cast the pipe id to strings */
	sprintf(resPipeBuffer, "%d%c", respondingPipe[WRITE], NULL);
	
	//Parameter 0: Program Name
	paramVector[0] = PLAYERPROCESSNAME;
	//Parameter 1: Pipe
	paramVector[1] = resPipeBuffer;
	//Parameter 3: Null
	paramVector[2] = NULL;
	err = execve("./Player", paramVector, NULL);
	if(err < 0){
		printf("Error in executing a player process. Terminating\n");
		exit(19);
	}//end if
}//end do_child

/****************************************************************************************
	do_game
	Simulates RPS for the designated number of rounds.
	Parameters:
				* int numRounds - the number of rounds.
	Returns:
				* N/A
*****************************************************************************************/
void do_game(int numRounds, int pipeList[][NUMPIPES], pid_t idList[]){
	int roundCount = 0, i, err, winner;
	char results[2][MAXBUFFERSIZE];
	int wins[NUMPLAYERS + 1]; //+1 for a draw tally, draw takes index 0
	for(i = 0; i < NUMPLAYERS + 1; i++) //Start all tallies at 0
		wins[i] = 0;
	
	while(roundCount < numRounds){
			//Signal show
			for(i = 0; i < NUMPLAYERS; i++){
				printf("Controller: issuing Show signal\n");
				err = kill(idList[i], SIGUSR1);
				if(err < 0){
					printf("Controller: Error in sending Show to player - %d\n", errno);
					exit(12);
				}//end if
			}//end for(i)
				
			//Wait for players to put choice into pipe.
			err = sleep(1);
			if(err < 0){
				printf("Controller: Error in sleep - %d\n", errno);
				exit(20);
			}//end if
			
			//Get results from pipes
			for(i = 0; i < NUMPLAYERS; i++)
				read(pipeList[i][READ], results[i], MAXBUFFERSIZE);
		
			//Do logic with results
			winner = evaluate(results[0][0], results[1][0]);
			wins[winner + 1]++; //Scale from -1 to 1 to 0 to 2
			
			++roundCount;
	}//end while
	
	printResults(wins);
	
	for(i = 0; i < NUMPLAYERS; i++){
		printf("Controller: Issuing done signal\n");
		err = kill(idList[i], SIGUSR2);
		if(err < 0){
			printf("Controller: Error in sending Done to player - %d\n", errno);
			exit(13);
		}//end if
	}//end for(i)
}//end do_game

/*******************************************************************************************
	evaluate
	Given two RPS choices, this function will:
		* Determine a winner
		* Return a char with the first letter of the winner.
	Parameters:
		* char choice1: The FIRST letter of the RPS choice for Player 0.
		* char choice2: The FIRST letter of the RPS choice for Player 1.
	Returns:
		* Number of the winning player (starting at 0)
			- Returns negative if draw
******************************************************************************************/
int evaluate(char player0, char player1){
	int result;
	if(player1 % player0){ //is not a draw
		if(  ( (NUMCHOICES + player0 - player1) % NUMCHOICES) % 2)
			result = 1;
		else
			result = 0;
		
		printf("Player %d wins!\n\n", result);
	}//end if
	else{ //is a draw
		result = -1;
		printf("The match was a draw!\n\n");
	}//end else
	return result;
}//end evaluate



/*****************************************************************************************
Signal handler for Ready signal
******************************************************************************************/
void readyHandler(int sig){
	printf("Controller: Received ready\n");
}//end readyHandler

/************************************************************************************
Sets up the Ready signal handler.
*************************************************************************************/
void setupHandler(){
	struct sigaction ready_act;
	int err;
	
	sigset_t mask;
	sigemptyset(&mask);
	
	ready_act.sa_handler = (void(*)(int))readyHandler;
	ready_act.sa_mask = mask;
	ready_act.sa_flags = 0;
	err = sigaction(SIGUSR1, &ready_act, NULL);
	if(err < 0){
		printf("controllerSignal: Error %d on sigaction.\n", errno);
		exit(5);
	}//end if
}//end setupHandler

void printResults(int wins[]){
	printf("Finished\nDraws: %d\nPlayer0 wins: %d\nPlayer1 wins: %d\n", wins[0], wins[1], wins[2]);
}//end printResults