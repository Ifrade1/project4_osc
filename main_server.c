#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 33336
#define  RED     "\x1b[31m"
#define  GREEN   "\x1b[32m"
#define  YELLOW  "\x1b[33m"
#define  BLUE    "\x1b[34m"
#define  MAGENTA "\x1b[35m"
#define  CYAN    "\x1b[36m"

void error(const char *msg) {
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	int room;					// room id for user
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

// print list of clisockfd's
void print_list() {
	USR *temp = head;
	printf("List:\n");
	while (temp != NULL) {
		printf("%d\n", temp->clisockfd);
		temp = temp->next;
	}
	printf("\n");
}

void add_tail(int newclisockfd) {
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->next = NULL;
		tail = tail->next;
	}
	print_list();
}

void remove_item(int clisockfd) {
	USR *prev = NULL;
	USR *temp = head;

	if (temp != NULL && temp->clisockfd == clisockfd) {
		head = head->next;
		free(temp);
		return;
	} else {
		while (temp != NULL && temp->clisockfd != clisockfd) {
			prev = temp;
			temp = temp->next;
		}

		if (temp == NULL) {
			return;
		}

		prev->next = temp->next;

		free(temp);
	}
}

int getRoom(int fromfd) {
	int room;

	USR* cur = head;
	while (cur != NULL) {
		if (cur->clisockfd == fromfd) {
			 room = cur->room;
			 break;
		}
		cur = cur->next;
	}

	return room;
}

void broadcast(int fromfd, char* message) {
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");
	char buffer[512];
	int room  = 0;

	// get the room ID from the user in the user list
	room = getRoom(fromfd);

	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd && cur->room == room) {
			// prepare message
			sprintf(buffer, "[ROOM %d] %s", room, message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
	int room_num;
} ThreadArgs;

void* thread_main(void* args) {
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nrcv;

	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		if (strstr(buffer, "JOIN ROOM") != NULL) {
			char *temp;
			temp = strtok(buffer, "JOIN ROOM ");
			int room = atoi(temp);
			printf("ID %d is changing their room to %d\n", clisockfd, room);
			USR * cur = head;
			while (cur != NULL) {
				if (clisockfd == cur->clisockfd) {
					cur->room = room;
					break;
				}
				cur = cur->next;
			}
		}

		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer);

		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	if (nrcv == 0) {
		remove_item(clisockfd);
		print_list();
	}

	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[]) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, (struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of connections = 5

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));
		add_tail(newsockfd); // add this new client to the client list

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");

		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0;
}
