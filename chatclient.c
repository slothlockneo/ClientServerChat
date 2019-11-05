/*
Programmer Name: Molly Johnson (johnsmol)
Program Name: ClientServerChat
Program Description: ClientServerChat is my program for completing the requirements of the cs372 Project 1
Assignment. This program works basically as a chat program between a server on one host (A) and a client on
another host (B). A connection will be established between host A and host B, and the client
and server can send messages back and forth to one another until the user (who is controlling
both the client and server) requests to quit the program, at which point the connection will be closed.
The server will then remain waiting on the port for any additional connections until a SIGINT is received.
The name of the server portion of the program is chatserve and the name of the client portion of the program
is chatclient. 
Course Name: CS 372 Intro to Networks
Last Modified: 11/04/19
*/

//include other header files
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <arpa/inet.h>

//macro definitions
#define MAX_MESSAGE_SIZE 501
#define MAX_MSG_PLUS_HANDLE 513
#define HOST_ADDRESS "flip1.engr.oregonstate.edu"
#define MAX_HANDLE_SIZE 11
#define TRUE 1
#define FALSE 0
#define QUIT "\\quit" 

/*
pre-condititwo two valid c strings
post-conditions: a bool will be returned, true if the strings matched and false if the strings didn't match
description: this function accepts two strings as arguments. It then uses strcmp to compare the strings
and returns true if the strings matched, false if the strings didn't match. (I made this function because I could
never remember if strcmp should return 0 for matching or not matching so felt this made it more explicit). This
function is adapted from my own work created for OSU CS 344 Winter 2019 assignment 3, last updated 3-3-19.
*/
int StringMatch(char *string1, char *string2){
	//use strcmp to see if string one and two are equal. if equal, strcmp will return 0.
	//strcmp will return non-zero if the two strings are not equal.
	if(strcmp(string1, string2) == 0){
		//if the two strings are a match, return true
		return TRUE;
	}
	//if the two strings are not a match, return false
	return FALSE;
}

/*
pre-conditions:
post-conditions:
description:
*/
int IsNewline(char *userInputIn){
	//call StringMatch to check if the user input string is just a newline character.
	if(StringMatch(userInputIn, "\n") == TRUE){
		//if it is, return true
		return TRUE;
	}
	//else, return false
	return FALSE;	
}

/*
pre-conditions:
post-conditions:
description:
*/  
int IntInputValidation(char *inputString){
	//create variables for char to be checked, loop counter, and length of the input string
	char asciiValue;
	int inputLength = strlen(inputString);
	int i;

	//loop through entire string so can check each char
	for(i = 0; i < inputLength; i++){
		//set temp char equal to the current string char
		asciiValue = inputString[i];

		//check if the current char is within the valid ascii values for ints 0-9 (i.e. ascii values 48-57)
		if((asciiValue < 48) || (asciiValue > 57)){
			//if ascii value of char being checked is outside of the range for integers, return false
			return FALSE;
		}
	}
	//if all of the chars in the string to be checked were valid integers, return true
	return TRUE;
}

/*
pre-conditions:
post-conditions:
description:
*/
int GetMessage(char *userMessageIn, char *userHandleIn){
	//create buffer of size MAX_CHARS (2048 as given in the assignment instructions plus one for null terminator)
	char *buffer;
	size_t bufsize = MAX_MSG_PLUS_HANDLE;
	size_t characters;
	int goodInput = TRUE;

	//malloc the buffer and check that malloc is successful
	buffer = (char *)malloc(bufsize * sizeof(char));

	//if buffer == NULL, malloc had an error. print error message and exit with value of 1.
	if(buffer == NULL){
		perror("GETLINE BUFFER ERROR, UNABLE TO ALLOCATE\n");
		exit(1);
	}

	//keep looping (by using while(1)) to get the line of user input. check for if getline returns -1
	//is used to make sure getline doesn't encounter any problems due to signals
	while(1){
		//call getline to get user input from stdin and put it in the buffer
		characters = getline(&buffer, &bufsize, stdin);

		//check if getline returned -1
		if(characters == -1){
			//if getline returned -1, use clearerr on stdin and let it loop back around
			clearerr(stdin);
		}
		else{
			//else if getline was successful (didn't return -1), go ahead and break out of loop
			break;
		}
	}

	if(IsNewline(buffer) == FALSE){
		if(strlen(buffer) > MAX_MESSAGE_SIZE){
			strcpy(userMessageIn, "Error");
			printf("Error, your message was too long. Please try again.\n");
			goodInput = FALSE;
		}
		else{
			//user entered a valid message, remove the newline char from the buffer, replacing it
			//with a null terminating character
			buffer[strcspn(buffer, "\n")] = '\0';

			//cat the buffer with the newline char removed into the userHandle string variable
			strcpy(userMessageIn, userHandleIn);
			strcat(userMessageIn, "> ");
			strcat(userMessageIn, buffer);
		}
	}
	else{
		strcpy(userMessageIn, "Error");
		printf("Error, you didn't enter a message, you only hit enter. Please try again.\n");
		goodInput = FALSE;
	}
	
	//free the buffer that was malloc'd for getline and set to NULL
	free(buffer);
	buffer = NULL;

	return goodInput;
}

/*
pre-conditions:
post-conditions:
description:
*/
void ArgCheck(int argCount, char *args[]){
    if (argCount != 3){
        fprintf(stderr, "Wrong number of arguments! Must enter the correct hostname and a valid port number. Start the program again.\n");
        fflush(stdout); exit(1);    
    }
	//using atoi to convert from string to int adapted from:
	//https://www.quora.com/How-do-I-convert-character-value-to-integer-value-in-c-language
	if ((!IntInputValidation(args[2])) || (atoi(args[2]) < 0 ) || (atoi(args[2]) > 65535)){
		fprintf(stderr, "You entered a negative number, string or otherwise invalid port number argument. Start the program again.\n");
       	fflush(stdout); exit(1);    
	}
	if (!StringMatch(args[1], HOST_ADDRESS)){
		fprintf(stderr, "Your host argument doesn't match the server host. Start the program again and use flip1.engr.oregonstate.edu as your host.\n");
       	fflush(stdout); exit(1);    
	}
}

/*
pre-conditions:
post-conditions:
description:
*/
int GetHandle(char *userHandleIn){
	//create buffer of size MAX_CHARS (2048 as given in the assignment instructions plus one for null terminator)
	char *buffer;
	size_t bufsize = MAX_HANDLE_SIZE;
	size_t characters;
	int goodInput = TRUE;

	//malloc the buffer and check that malloc is successful
	buffer = (char *)malloc(bufsize * sizeof(char));

	//if buffer == NULL, malloc had an error. print error message and exit with value of 1.
	if(buffer == NULL){
		perror("GETLINE BUFFER ERROR, UNABLE TO ALLOCATE\n");
		exit(1);
	}

	//keep looping (by using while(1)) to get the line of user input. check for if getline returns -1
	//is used to make sure getline doesn't encounter any problems due to signals
	while(1){
		//call getline to get user input from stdin and put it in the buffer
		characters = getline(&buffer, &bufsize, stdin);

		//check if getline returned -1
		if(characters == -1){
			//if getline returned -1, use clearerr on stdin and let it loop back around
			clearerr(stdin);
		}
		else{
			//else if getline was successful (didn't return -1), go ahead and break out of loop
			break;
		}
	}

	if(IsNewline(buffer) == FALSE){
		if(strlen(buffer) > MAX_HANDLE_SIZE){
			strcpy(userHandleIn, "Error");
			printf("Error, your handle was too long. Please try again with 10 characters or less.\n");
			goodInput = FALSE;
		}
		else{
		//user entered a valid handle, remove the newline char from the buffer, replacing it
		//with a null terminating character
		buffer[strcspn(buffer, "\n")] = '\0';

		//copy the buffer with the newline char removed into the userHandle string variable
		strcpy(userHandleIn, buffer);
		}
	}
	else{
		strcpy(userHandleIn, "Error");
		printf("Error, you didn't enter a handle, you only hit enter. Please try again.\n");
		goodInput = FALSE;
	}
	
	//free the buffer that was malloc'd for getline and set to NULL
	free(buffer);
	buffer = NULL;

	return goodInput;
}

/*
pre-conditions:
post-conditions:
description:
*/
void SendMessage(int sockFD, char *sendBuf){
	int charsW;
	charsW = send(sockFD, sendBuf, strlen(sendBuf), 0);
	if (charsW < 0){
		fprintf(stderr, "Error writing to socket.\n"); fflush(stdout); exit(1);
	}
	if (charsW < strlen(sendBuf)){
		fprintf(stderr, "Warning: some, but not all data written to socket.\n"); fflush(stdout); exit(1);
	}

	int checkSend = -5;
	do{
		ioctl(sockFD, TIOCOUTQ, &checkSend);
	} while(checkSend > 0);

	if (checkSend < 0){
		fprintf(stderr, "Ioctl error.\n"); fflush(stdout); exit(1);
	}
}

/*
pre-conditions:
post-conditions:
description:
*/
int main(int argc, char *argv[]){
    ArgCheck(argc, argv);
		
	//using atoi to convert from string to int adapted from:
	//https://www.quora.com/How-do-I-convert-character-value-to-integer-value-in-c-language
	char const *portNum = argv[2];
	char *hostAddress = argv[1];
	int goodHandle = FALSE;
	int goodMessage = FALSE;
	char userHandle[MAX_HANDLE_SIZE];
	char sendBuffer[MAX_MSG_PLUS_HANDLE];
	char recvBuffer[MAX_MSG_PLUS_HANDLE];
	memset(userHandle, '\0', sizeof(userHandle));
	memset(sendBuffer, '\0', sizeof(sendBuffer));
	memset(recvBuffer, '\0', sizeof(recvBuffer));

	int status, socketFD, statusConnect, charsRead;
	struct addrinfo hints;
	struct addrinfo *servinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	while (goodHandle == FALSE){
		printf("What's your handle?: ");
		fflush(stdin);
		goodHandle = GetHandle(userHandle);
	}
	//printf("the user handle is: %s\n", userHandle);

	status = getaddrinfo(hostAddress, portNum, &hints, &servinfo);
	if (status < 0){
		fprintf(stderr, "Error getting address info.\n"); fflush(stdout); exit(1);
	}	

	do{
		socketFD = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
		if (socketFD < 0){
			fprintf(stderr, "Error creating socket descriptor.\n"); fflush(stdout); exit(1);
		}	

		statusConnect = connect(socketFD, servinfo->ai_addr, servinfo->ai_addrlen);
		if (statusConnect < 0){
			fprintf(stderr, "Error connecting to server.\n"); fflush(stdout); exit(1);
		}
		while (goodMessage == FALSE){
			printf("%s> ", userHandle);
			fflush(stdin);
			goodMessage = GetMessage(sendBuffer, userHandle);
		}	

		goodMessage = FALSE;
		SendMessage(socketFD, sendBuffer);

		charsRead = recv(socketFD, recvBuffer, sizeof(recvBuffer) - 1, 0);
		if (charsRead < 0){
			fprintf(stderr, "Error reading from the socket.\n"); fflush(stdout); exit(1);
		}
	
		if ((strstr(sendBuffer, QUIT) == NULL) && (strstr(recvBuffer, QUIT) == NULL)){
			printf("%s\n", recvBuffer); fflush(stdout);
			memset(sendBuffer, '\0', sizeof(sendBuffer));
			memset(recvBuffer, '\0', sizeof(recvBuffer));
		}
		if (strstr(recvBuffer, QUIT) != NULL){
			printf("%s\n", recvBuffer); fflush(stdout);
			printf("the client is disconnecting.\n");
		}
		if (strstr(sendBuffer, QUIT) != NULL){
			printf("the client is disconnecting.\n");
		}
		close(socketFD);
	} while((strstr(sendBuffer, QUIT) == NULL) && (strstr(recvBuffer, QUIT) == NULL));

	freeaddrinfo(servinfo);
    return 0;
}
