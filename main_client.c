#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define PORT_NUM 33336


#define  RED     "\x1b[31m"
#define  GREEN   "\x1b[32m"
#define  YELLOW  "\x1b[33m"
#define  BLUE    "\x1b[34m"
#define  MAGENTA "\x1b[35m"
#define  CYAN    "\x1b[36m"
/** User name the client reports to the server */
char user_name[25];
int room_num;

void color(int random){
	switch(random){
		case 0:
			printf("\x1b[0m");
			break;
		case 1:
			printf(RED "");
			break;
		case 2:
			printf(GREEN "");
			break;
		case 3:
			printf(YELLOW "");
			break;
		case 4:
			printf(BLUE "");
			break;
		case 5:
			printf(MAGENTA "");
			break;
		case 6:
			printf(CYAN "");
			break;
	}
}
void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;
/**
 * Thread function that receives and prints messages from other clients
 */
void* thread_main_recv(void* args) {
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[512];
	int n;

	n = recv(sockfd, buffer, 512, 0);
	while (n > 0) {
		memset(buffer, 0, 512);
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR recv() failed");
		printf("\n%s\n", buffer);
	}

	return NULL;
}
/**
 * Thread function that handles user input and sending messages to the server
 */
void* thread_main_send(void* args) {
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep sending messages to the server
	char buffer[256];
	// Message that is sent to the server
	char message[512];
	int n;

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		memset(buffer, 0, 256);
		memset(message, 0, 512);
		fgets(buffer, 256, stdin);

		if (strlen(buffer) == 1) break;

		sprintf(message, "[%s]: %s\0", user_name, buffer);
		n = send(sockfd, message, strlen(message), 0);
		if (n < 0) error("ERROR writing to socket");
	}

	return NULL;
}

int main(int argc, char *argv[]) {
	int arg = 0;			// 0 connect normally, 1 get room list and prompt, 2 create new room
	int valid_connection = 0;	// Whether the server actually is going to let us connect, sent by the server
	srand(time(NULL));
	int pick = rand() % 7;
	color(pick);
	if (argc < 2) error("Please speicify hostname");
	if (argc == 3) {
		if (strcmp(argv[2], "new") == 0) {
			room_num = -1;
			printf("Attempting to join new room\n");
			arg = 2;
		} else {
			room_num = atoi(argv[2]);
			printf("Attempting to join room #%d\n", room_num);
			arg = 0;
		}
	} else {
		room_num = 0;
		printf("No room specified, Getting room list...\n");
		arg = 1;
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, (struct sockaddr *) &serv_addr, slen);

	if (status < 0) {
		error("ERROR connecting");
		return -1;
	} else {
		char buf[64];
		if (arg == 0) {
			char buffer[255];
			// Tell the server to assign the user a room	
			sprintf(buf, "JOIN ROOM %d", room_num);
			int n = send(sockfd, buf, 64, 0);
			if (n < 0) printf("Could not connect with the room!");
			n = send(sockfd, buf, 64, 0);
			if (n < 0) {
				printf("Could not connect with the room!");
				return -1;
			}
			// wait for response
			recv(sockfd, buffer, 255, 0);
			printf("%s\n", buffer);
			// Server disconnects us but we can still send a message and set
			// a username, so we use this to gracefully stop the program before that happens
			if (strstr(buffer, "ERROR") != NULL)
				valid_connection = -1;
		} else if (arg == 1) {
			sprintf(buf, "!LIST ROOM\n");
			char buffer[255];
			char room_get[16];
			// send the room command then wait for a reply
			int n = send(sockfd, buf, 64, 0);
			n = recv(sockfd, buffer, 255, 0);
			printf("%s\n", buffer);
			printf("Enter room to join: ");
			fgets(room_get, 16, stdin);

			room_num = atoi(room_get);
			sprintf(buf, "JOIN ROOM %d", room_num);
			n = send(sockfd, buf, 64, 0);
			if (n < 0) {
				printf("Could not connect with the room!");
				return -1;
			}
			// wait for response
			recv(sockfd, buffer, 255, 0);
			printf("%s\n", buffer);
			// Server disconnects us but we can still send a message and set
			// a username, so we use this to gracefully stop the program before that happens
			if (strstr(buffer, "ERROR") != NULL)
				valid_connection = -1;
		} else if (arg == 2) {
			char buffer[255];
			// Tell server to create a new room
			sprintf(buf, "!NEW ROOM");
			int n = send(sockfd, buf, 64, 0);
			recv(sockfd, buffer, 255, 0);
			printf("%s\n", buffer);
			if (strstr(buffer, "ERROR") != NULL)
				valid_connection = -1;
		}
		
	}
	if (valid_connection == -1)
	{
		printf("Kicked off from the server.\n");
		return -1;
	}
	// Get the username the user wants
	printf("Enter a username: ");
	fgets(user_name, 25, stdin);
	strtok(user_name, "\n");
	if (strlen(user_name) == 1) {
		printf("No username specified, exiting...\n");
		return 1;
	} else {
		printf("Username set to %s\n", user_name);
	}

	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);

	close(sockfd);

	return 0;
}
