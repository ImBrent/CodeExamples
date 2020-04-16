/******************************************************************************
	player.c
	Implementation for the players in the RPS game.
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "controller.h"
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <time.h>

void showHandler(int sig);
void doneHandler(int sig);
void signalReady(pid_t parent);
void playingRPS();
void getChoice(char* buff);
void setupHandlers();

bool playing = true;
int playerNum;
int outPipe;

int main(int argc, char** argv){
	int err;
	pid_t myPid, parentPid;
	
	setupHandlers();
	
	/**** Get pids ****/
	parentPid = getppid();
	myPid = getpid();
	srand(1000 * myPid); //Ensure that both children have unique seeds
	playerNum = myPid&1;
	outPipe = atoi(argv[1]);
	
	/********** Signaling that the player is ready. ************/
	signalReady(parentPid);
	
	/*********** Playing RPS **********************/
	playingRPS();
	
	/*********** Done playing, terminate normally ***************/
	
	
	printf("Player %d: terminating\n", playerNum);
	return 0;
}//end main

/***********************************************************************************
Signal handler for Show signal
***********************************************************************************/
void showHandler(int sig){
	char choiceBuffer[2];
	int err;
	getChoice(choiceBuffer);
	err = write(outPipe, choiceBuffer, 2);
	if(err < 0){
		printf("Player.c: error in showHandler - %d\n", errno);
		exit(11);
	}//end if
	
}//end showHandler

/***********************************************************************************
	Signal handler for Done signal.
	Sets the playing variable to false, which will allow the playingRPS function
	to return normally.
***********************************************************************************/
void doneHandler(int sig){
	printf("Player %d: Done called\n", playerNum);
	playing = false;
}//end doneHandler

/**********************************************************************************
Sets up the signal handlers for the signals to be used by the player.
***********************************************************************************/
void setupHandlers(){
	struct sigaction show_act;
	struct sigaction done_act;
	int err;
	sigset_t mask;
	sigemptyset(&mask); //Create mask, initialize to empty
	
	show_act.sa_handler = (void(*)(int))showHandler;
	show_act.sa_mask = mask;
	show_act.sa_flags = 0;
	err = sigaction(SIGUSR1, &show_act, NULL);
	if(err < 0){
		printf("player.c: Error %d on sigaction.\n", errno);
		exit(7);
	}//end if

	done_act.sa_handler = (void(*)(int))doneHandler;
	done_act.sa_mask = mask;
	done_act.sa_flags = 0;
	err = sigaction(SIGUSR2, &done_act, NULL);
	if(err < 0){
		printf("player.c: Error %d on sigaction.\n", errno);
		exit(8);
	}//end if
}//end setupHandlers

/***************************************************************************
Sleeps for a second, then signals to the parent that the player is ready.
***************************************************************************/
void signalReady(pid_t parent){
	int err;
	sleep(playerNum); //Allow controller to reach Pause, ensure that not signaling
					  //At the same time.
	printf("Player %d issuing Ready signal\n", playerNum);
	err = kill(parent, SIGUSR1);
	if(err < 0){
		printf("player.c: Error in sending Ready signal to parent: %d\n", errno);
		exit(6);
	}//end if
}//end signalReady

/******************************************************************************
	Executes until the controller sends the done signal.
******************************************************************************/
void playingRPS(){
	while(playing)
		pause(); //Wait for "Show" signal

}//end playingRPS

/****************************************************************************
	Gets a choice for RPS
	Parameters: A buffer to store the choice in.
	Returns: The name of the choice (By reference).
******************************************************************************/
void getChoice(char* buff){
	switch(rand() % 3){
		case 0:
			printf("Player %d: Selects Rock\n", playerNum);
			sprintf(buff, "0");
			break;
		case 1:
			printf("Player %d: Selects Paper\n", playerNum);
			sprintf(buff, "1");
			break;
		default:
			printf("Player %d: Selects Scissors\n", playerNum);
			sprintf(buff, "2");
	}//end switch
}//end getChoice