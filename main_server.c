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
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define MAX_ROOMS 5
#define MAX_USERS 5
#define MAX_CONNECTIONS MAX_ROOMS * MAX_USERS
void error(const char *msg) {
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	int room;			// room id for user
	struct _USR* next;	// for linked list queue
} USR;
// Head node must always exist
USR *head = NULL;

/* Room idx corresponds to room number
 * value corresponds to number of users in room
 * -1 for empty room or invalid
 */
int rooms[MAX_ROOMS];

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
/**
  * Add a tail to the end of the linked list
  */
void add_tail(int newclisockfd) {
	printf("Socket %d connected.\n", newclisockfd);
	// If head does not exist yet
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->next = NULL;
		head->room = NULL;
	} else {
		// find end of list (easier to maintain than keeping separate tail variable)
		USR * cur = head;
		while (cur->next != NULL) {
			cur = cur->next;
		}
		USR * tmp = (USR*) malloc(sizeof(USR));
		tmp->clisockfd = newclisockfd;
		tmp->room = -1;
		tmp->next = NULL;
		cur->next = tmp;
	}
	print_list();
}
/**
  * Removes a node from the linked list
  */
void remove_item(int clisockfd) {
	USR *prev = NULL;
	USR *cur = head;

	// No clients connected, do nothing
	if (cur == NULL) {
		return;
	} 

	// Traverse through the linked list
	// Find the desired ID
	while(cur != NULL) {
		// Entity found, remove the link
		if (cur->clisockfd == clisockfd) {
			printf("Socket %d disconnected.\n", clisockfd);
			// If entity is the first
			if (cur == head) {
				if (cur->next == NULL) 
				{
					free(head);
					head = NULL;
				} else {
					head = cur->next;
					free(cur);
				}
			// Otherwise set prev.next to the current.next to destroy cur
			} else {		
				prev->next = cur->next;
				free(cur);
			}
			break;
		}
		// Set prev to cur and cur to cur.next
		prev = cur;
		cur = cur->next;
	}
	print_list();
	return;
}

void initRooms() {
	for (int i = 0; i < MAX_ROOMS; i++) {
		rooms[i] = -1;
	}
}

/**
 * Decrement the user count for a room for when a user leaves the chat
 */
void updateRooms(int clisockfd) {
	// when user is removed update the rooms
	int room = -1;
	USR *cur = head;
	while (cur != NULL) {
		if (cur->clisockfd == clisockfd) {
			room = cur->room;
			break;
		}
		cur = cur->next;
	}
	if (room < 0) {
		printf("User does not belong to a room somehow...\n");
		return;
	}
	rooms[room]--;
	if (rooms[room] <= 0) {
		rooms[room] = -1;
	}
	printf("Removed Socket %d from room %d.\n", clisockfd, room);
}

void listRooms() {
	// list the rooms and the number of people in them
	// print to client terminal:("Server says following options are available:\n")
	for (int i = 0; i < MAX_ROOMS; i++) {
		if (rooms[i] != -1 && rooms[i] != 0) {
			// print to client terminal: ("Room %d: %d people\n", rooms[i][0], rooms[i][1])
		}
	}
}
/**
  * Traverse through linked list looking for the socket ID
  * return the ID of the room the user is in, -1 if not found
  */
int getRoom(int fromfd) {
	int room = -1;

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

/**
  * Thread for receiving messages from clients
  * each client gets their own thread
  */
void* thread_main(void* args) {
	// Exit status determines what functions are executed at disconnect
	int exit_status = 0;
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nrcv = 999;
	memset(buffer, 0, 255);

	//nrcv = recv(clisockfd, buffer, 255, 0);
	//if (nrcv < 0) error("ERROR recv() failed");
	char send_buffer[255];
	memset(send_buffer, 0, 255);

	while (nrcv > 0) {
		if (strstr(buffer, "JOIN ROOM") != NULL) {
			printf("User is attempting to join a room.\n");
			printf("%s\n", buffer);
			char *temp;
			temp = strtok(buffer, "JOIN ROOM ");
			int room_num = atoi(temp);
			// Check if the room is valid before allowing to join
			if (room_num >= MAX_ROOMS) {
				printf("ERROR -3: User tried to join an invalid room, disconnecting...\n");
				sprintf(send_buffer, "ERROR -3: Room is outside of the max boundries.\n");
				send(clisockfd, send_buffer, 255, 0);
				exit_status = -1;
				break;
			}
			if (rooms[room_num] != -1) {
				rooms[room_num]++;
				printf("ID %d is changing their room to %d\n", clisockfd, room_num);
				USR * cur = head;
				while (cur != NULL) {
					if (clisockfd == cur->clisockfd) {
						cur->room = room_num;
						break;
					}
					cur = cur->next;
				}
				sprintf(send_buffer, "Welcome to room #%d.\n", room_num);
				send(clisockfd, send_buffer, 255, 0);
			} else if (rooms[room_num] >= MAX_USERS) {
				// Room is invalid disconnect the user
				sprintf(send_buffer, "ERROR -1: Room is full, please connect to a different room\n");
				send(clisockfd, send_buffer, 255, 0);
				exit_status = -1;
				break;
			} else {
				sprintf(send_buffer, "ERROR -2: Room is currently not registered.\nConnect with the 'new' keyword.\n");
				send(clisockfd, send_buffer, 255, 0);
				exit_status = -1;
				break;
			}
		
		} else if (strstr(buffer, "!LIST ROOM") != NULL) {
			// Send a list of rooms to the client
			// Create variables for a string builder
			char buf[255];		// String buffer
			int i = 0;  		// Current index in buffer
			memset(&buf, 0, 255);
			sprintf(&buf[i], "ROOM LIST:\n");
			i += 11;
			// Loop through each room, check if valid, and append to list
			for (int j = 0; j < MAX_ROOMS; j++) {
				if (rooms[j] != -1) {
					sprintf(&buf[i], "ROOM %2d : %2d Users\n", j, rooms[j]);
					i += 19;
				}
			}
			send(clisockfd, buf, 255, 0);
		} else if (strstr(buffer, "!NEW ROOM") != NULL) {
			// Find the first empty room and assign it to the user
			printf("User wants to create a new room...\n");
			int room = -1;
			for (int i = 0; i < MAX_ROOMS; i++) {
				if (rooms[i] == -1) {
					room = i;
					break;
				}
			}
			if (room == -1) {
				// No empty rooms avaliable, tell the client
				sprintf(send_buffer, "No empty rooms, please join an existing room.\n");
				send(clisockfd, send_buffer, 255, 0);
				// close the connection
				exit_status = -1;
				break;

			} else {
				// assign the user the room
				rooms[room] = 1;
				sprintf(send_buffer, "Assigning to Room %d.\n", room);
				send(clisockfd, send_buffer, 255, 0);
				
				USR * cur = head;
				while (cur != NULL) {
					if (clisockfd == cur->clisockfd) {
						cur->room = room;
						break;
					}
					cur = cur->next;
				}
			}
		} else if (strlen(buffer) == 1) {
			printf("User is disconnecting...\n");
			break;
		} else {
			// Normal message, send to everyone in the same room
			// we send the message to everyone except the sender
			broadcast(clisockfd, buffer);
		}
		// Clear the buffer before receiving anything else
		memset(buffer, 0, 255);
		memset(send_buffer, 0, 255);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}
	// Run this only if the user was actually assigned a room
	if (exit_status == 0)  {
		updateRooms(clisockfd);
	}
	remove_item(clisockfd);
	close(clisockfd);
	printf("Destroying thread.\n");
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

	initRooms(); // initialize all rooms to -1

	listen(sockfd, 5); // maximum number of connections = 5
	printf("Starting chat server...\n");
	head = (USR *) malloc(sizeof(USR));
	head->clisockfd = -5555;
	head->room = -5555;
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
