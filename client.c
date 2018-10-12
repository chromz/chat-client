#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>

struct sockaddr_in serv;
int fd;
int connection;
char msg[100] = "";

int main () {
	fd = socket(AF_INET, SOCK_STREAM, 0);
	serv.sin_family = AF_INET;
	serv.sin_port = htons(8096);

	inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

	connect(fd, (struct sockaddr * )&serv, sizeof(serv));

	while(1) {
		printf("Enter a message: ");
		fgets(msg, 100, stdin);
		send(fd, msg, strlen(msg), 0);
	}
}
