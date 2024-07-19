#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFF_SIZE 1024

int main() {
	setbuf(stdout, NULL);

	printf("Logs from your program will appear here!\n");

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	printf("Client connected\n");

	char buff[BUFF_SIZE] = "";
	if (recv(client_fd, buff, sizeof(buff), 0) == -1) {
		printf("Receiving failed: %s\n", strerror(errno));
		return 1;
	}

	char verb[BUFF_SIZE] = "";
	char path[BUFF_SIZE] = "";
	sscanf(buff, "%s %s", verb, path);

	printf("verb %s\n", verb);
	printf("path %s\n", path);

	char *ok = "HTTP/1.1 200 OK\r\n\r\n";
	char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
	char *resp = ok;
	if (strncmp(path, "/", strlen(path)) != 0) {
		resp = not_found;
	}

	if (send(client_fd, resp, strlen(resp), 0) == -1) {
		printf("Sending failed: %s\n", strerror(errno));
		return 1;
	}
	close(server_fd);

	return 0;
}
