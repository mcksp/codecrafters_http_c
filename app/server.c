#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFF_SIZE 1024

struct header {
	char name[BUFF_SIZE];
	char value[BUFF_SIZE];
};

char* header_val(char* name, struct header* headers, int headers_len) {
	for (int i = 0; i < headers_len; i++) {
		if (strncmp(headers[i].name, name, BUFF_SIZE) == 0) {
			return headers[i].value;
		}
	}
	return NULL;
}

void handle_request(int client_fd) {
	char buff[BUFF_SIZE] = "";
	char *curr = buff;
	char *next = buff;
	if (recv(client_fd, buff, sizeof(buff), 0) == -1) {
		printf("Receiving failed: %s\n", strerror(errno));
		return;
	}
	buff[BUFF_SIZE - 1] = 0;

	strtok_r(curr, "\r\n", &next);
	char status_line[BUFF_SIZE] = "";
	strncpy(status_line, curr, BUFF_SIZE);
	curr = next + 1;

	char verb[BUFF_SIZE] = "";
	char path[BUFF_SIZE] = "";
	char protocol[BUFF_SIZE] = "";
	sscanf(status_line, "%s %s %s", verb, path, protocol);

	printf("verb %s\n", verb);
	printf("path %s\n", path);

	struct header headers[32];
	int headers_len = 32;
	for(int i = 0; i < 32; i++) {
		strtok_r(curr, "\r\n", &next);
		if (strlen(curr) > 4) {
			char *val = NULL;
			strtok_r(curr, ":", &val);
			strncpy(headers[i].name, curr, BUFF_SIZE);
			strncpy(headers[i].value, val + 1, BUFF_SIZE);
		} else {
			curr = next + 1;
			headers_len = i;
			break;
		}
		curr = next + 1;
	}

	for (int i = 0; i < headers_len; i++) {
		printf("name: %s, value: %s\n", headers[i].name, headers[i].value);
	}
	

	char *ok = "HTTP/1.1 200 OK";
	char *not_found = "HTTP/1.1 404 Not Found";
	char *status = ok;
	char body[BUFF_SIZE] = "";
	char resp[BUFF_SIZE * 5] = "";

	if (strncmp(path, "/echo/", 6) == 0) {
		strncpy(body, path + 6, BUFF_SIZE);
	} else if (strncmp(path, "/user-agent", BUFF_SIZE) == 0) {
		char *val = header_val("User-Agent", headers, headers_len);
		if (val != NULL) {
			strncpy(body, val, BUFF_SIZE);
		} else {
			strncpy(body, "no user-agent provided", BUFF_SIZE);
		}
	} else if (strncmp(path, "/", BUFF_SIZE) != 0) {
		status = not_found;
	}

	strcat(resp, status);
	strcat(resp, "\r\n");
	strcat(resp, "Content-Type: text/plain\r\n");
	char content_length_header[BUFF_SIZE] = "";
	sprintf(content_length_header, "Content-Length: %lu\r\n", strlen(body));
	strcat(resp, content_length_header);
	strcat(resp, "\r\n");
	strcat(resp, body);

	if (send(client_fd, resp, strlen(resp), 0) == -1) {
		printf("Sending failed: %s\n", strerror(errno));
	}
}

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
	
	while(1) {
		client_addr_len = sizeof(client_addr);
		int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		printf("Client connected\n");

		pid_t p = fork();

		if(p < 0) {
			printf("fork failed: %s\n", strerror(errno));
			return 1;
		}

		if (p == 0) {
			handle_request(client_fd);
			return 0;
		}
	}

	close(server_fd);
	return 0;
}
