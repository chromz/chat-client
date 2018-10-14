#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <netinet/in.h>
#include <errno.h>

#define MESSAGE_BUFFER 500
#define USERNAME_BUFFER 10

typedef struct {
	char* prompt;
	int socket;
} thread_data;

// metodo para conectarse al servidor
void * connect_to_server(int socket_fd, struct sockaddr_in *address) {
	int response = connect(socket_fd, (struct sockaddr *) address, sizeof *address);
	if (response < 0) {
		fprintf(stderr, "connect failed: %s\n", strerror(errno));
		exit(1);
	} else {
		printf("Connected");
	}
}

// metodo para enviar un mensaje al servidor
void * send_message(char prompt[USERNAME_BUFFER+4], int socket_fd, struct sockaddr_in *address) {
	printf("%s", prompt);
	char message[MESSAGE_BUFFER];
	char final_message[MESSAGE_BUFFER+USERNAME_BUFFER+1];
	while(fgets(message, MESSAGE_BUFFER, stdin) != NULL) {
		memset(final_message,0,strlen(final_message));
		strcat(final_message, prompt);
		strcat(final_message, message);
		printf("\n%s", prompt);
		if (strncmp(message, "/quit", 5) == 0) {
			printf("Closing connection...\n");
			exit(0);
		}
		send(socket_fd, final_message, strlen(final_message)+1, 0);
	}
}

void * receive (void * threaddata) {
	int socket_fd, response;
	char message[MESSAGE_BUFFER];
	thread_data* pdata = (thread_data*)threaddata;
	socket_fd = pdata->socket;
	char* prompt = pdata -> prompt;
	memset(message, 0, MESSAGE_BUFFER);

	while(true) {
		response = recvfrom(socket_fd, message, MESSAGE_BUFFER, 0, NULL, NULL);
		if (response == -1) {
			printf("recv failed");
			break;
		} else if (response == 0) {
			printf("\nfriend disconnected\n");
			break;
		} else {
			printf("\nServer> %s", message);
			printf("%s", prompt);
			fflush(stdout);
		}
	}

}

int main (int argc, char**argv) {
	if (argc < 2) {
		printf("Uso: client [ip] [puerto] \n");
		exit(1);
	}
	
	long port = strtol(argv[2], NULL, 10);
	struct sockaddr_in serv, cl_addr;
	char * server_address;
	int fd, response;
	char prompt[USERNAME_BUFFER+4];
	char username[USERNAME_BUFFER];

	pthread_t thread;
//	int connection;
//	char msg[100] = "";

	printf("Enter your username: ");
	fgets(username, USERNAME_BUFFER, stdin);
	username[strlen(username) - 1] = 0;
	strcpy(prompt, username);
	strcat(prompt, "> ");

	server_address = argv[1];
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = inet_addr(server_address);
	serv.sin_port = htons(port);
	fd = socket(AF_INET, SOCK_STREAM, 0);

//	inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
//	connect(fd, (struct sockaddr * )&serv, sizeof(serv));
	connect_to_server(fd, &serv);

	thread_data data;
	data.prompt = prompt;
	data.socket = fd;

	pthread_create(&thread, NULL, receive, (void *) &data);
	send_message(prompt, fd, &serv);

	close(fd);
	pthread_exit(NULL);
	return 0;

/*
	while(1) {
		printf("Enter a message: ");
		fgets(msg, 100, stdin);
		send(fd, msg, strlen(msg), 0);
	}
*/
}
