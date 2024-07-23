#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "zlib.h"

#define BUFF_SIZE 1024

char *directory = NULL;

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

void response(char *status, char *content_type, char *body, char *encoding, int socket) {
	FILE *socketf = fdopen(socket, "w");
	fprintf(socketf, "%s\r\n", status);

	if (content_type != NULL) {
		fprintf(socketf, "Content-Type: %s\r\n", content_type);
	}
	char *next = NULL;
	char *curr = NULL;
	int gzip = 0;
	char zipped[BUFF_SIZE] = "";

	if (encoding != NULL) {
		curr = strtok_r(encoding, ", ", &next);
	}
	while (curr != NULL) {
		if (strncmp(curr, "gzip", BUFF_SIZE) == 0) {
			fprintf(socketf, "Content-Encoding: gzip\r\n");
			gzip = 1;
			break;
		}

		curr = strtok_r(NULL, ", ", &next);
	}

	if (gzip && body != NULL) {
		z_stream defstream;
		defstream.zalloc = Z_NULL;
		defstream.zfree = Z_NULL;
		defstream.opaque = Z_NULL;

		defstream.avail_in = strlen(body) + 1;
		defstream.next_in = (Bytef *) body;		
		defstream.avail_out = BUFF_SIZE;
		defstream.next_out = (Bytef *) zipped;

		deflateInit(&defstream, Z_BEST_COMPRESSION);
		deflate(&defstream, Z_FINISH);
		deflateEnd(&defstream);
		body = zipped;
	}

	printf("hejka\n");

	if (body != NULL) {
		fprintf(socketf, "Content-Length: %lu\r\n", strlen(body));
	}

	fprintf(socketf, "\r\n");

	if (body != NULL) {
		fputs(body, socketf);
	}

	fclose(socketf);
	close(socket);
	printf("koniec\n");
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
		if (curr[0] != '\r') {
			char *val = NULL;
			strtok_r(curr, ":", &val);
			strncpy(headers[i].name, curr, BUFF_SIZE);
			strncpy(headers[i].value, val + 1, BUFF_SIZE);
		} else {
			headers_len = i;
			break;
		}
		curr = next + 1;
	}
	char *payload = curr + 2;

	char *encoding = header_val("Accept-Encoding", headers, headers_len);

	for (int i = 0; i < headers_len; i++) {
		printf("name: %s, value: %s\n", headers[i].name, headers[i].value);
	}
	

	char *ok = "HTTP/1.1 200 OK";
	char *created = "HTTP/1.1 201 Created";
	char *bad_request = "HTTP/1.1 400 Bad Request";
	char *not_found = "HTTP/1.1 404 Not Found";
	char *internal = "HTTP/1.1 500 Internal Server Error";

	char *type_text = "text/plain";
	char *type_octet = "application/octet-stream";

	char body[BUFF_SIZE] = "";
	char resp[BUFF_SIZE * 5] = "";

	if (strncmp(path, "/echo/", 6) == 0) {
		response(ok, type_text, path + 6, encoding, client_fd);
	} else  if (strncmp(path, "/files/", 7) == 0) {
		char filename[BUFF_SIZE] = "";
		strncpy(filename, path + 7, BUFF_SIZE);
		if (directory == NULL) {
			printf("no directory specified\n");
			return;
		}

		char filepath[BUFF_SIZE] = "";
		snprintf(filepath, BUFF_SIZE, "%s%s", directory, filename);

		if (strncmp(verb, "GET", BUFF_SIZE) == 0) {
			FILE *f = fopen(filepath, "r");
			if (f == NULL) {
				printf("couldn't find file: %s\n", filepath);
				response(not_found, type_text, "file not found", encoding, client_fd);
				return;
			}
			fseek(f, 0, SEEK_END);
			long file_size = ftell(f);
			fseek(f, 0, SEEK_SET);

			printf("file size: %ld\n", file_size);
			char *file_content = calloc(file_size + 1, sizeof(char));
			fread(file_content, sizeof(char), file_size, f);
			file_content[file_size] = 0;
			fclose(f);

			response(ok, type_octet, file_content, encoding, client_fd);
		} else if (strncmp(verb, "POST", BUFF_SIZE) == 0) {
			FILE *f = fopen(filepath, "w");
			if (f == NULL) {
				printf("couldn't find file: %s\n", filepath);
				response(internal, type_text, "couldn't save file", encoding, client_fd);
				return;
			}
			fwrite(payload, sizeof(char), strlen(payload), f);
			fclose(f);
			response(created, NULL, NULL, encoding, client_fd);
		} else {
			response(not_found, NULL, NULL, encoding, client_fd);
		}
	} else if (strncmp(path, "/user-agent", BUFF_SIZE) == 0) {
		char *val = header_val("User-Agent", headers, headers_len);
		if (val != NULL) {
			response(ok, type_text, val, encoding, client_fd);
		} else {
			response(bad_request, type_text, "User-Agent header not found", encoding, client_fd);
		}
	} else if (strncmp(path, "/", BUFF_SIZE) == 0) {
		response(ok, type_text, "ok", encoding, client_fd);
	} else {
		response(not_found, NULL, NULL, encoding, client_fd);
	}
}

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);

	printf("Logs from your program will appear here!\n");
	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], "--directory") == 0 && strlen(argv[i + 1]) > 0) {
			directory = argv[i + 1];
			break;
		}
	}

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
