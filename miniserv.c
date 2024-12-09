#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


int server_fd; 
char buffer[300000];
char message[300000];
int client_socks[1000];
int client_ids[1000];
int last_id = 0;
struct sockaddr_in address;
socklen_t addrlen;

void setup(int port) {
	bzero(client_socks, sizeof(client_socks));
	bzero(client_ids, sizeof(client_ids));
	bzero(buffer, sizeof(buffer));
	bzero(message, sizeof(message));

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		write(2, "Fatal error\n", 13);
		exit(1);
	}

	// create addr
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(port);
	socklen_t addrlen = sizeof(address); 

	// bind socket to addr
	if (bind(server_fd, (const struct sockaddr *)&address, addrlen) < 0) {
		write(2, "Fatal error\n", 13);
		exit(1);
	}

	// listen
	if (listen(server_fd, 10) < 0) {
		write(2, "Fatal error\n", 13);
		exit(1);
	}
}

void send_to_all(char *message, int except_fd) {
	for (int i = 0; i < 1000; i++) {
		if (client_socks[i] != except_fd && client_socks[i] > 0) {
			send(client_socks[i], message, strlen(message), 0);
		}
	}
}

void handle_lines(char *buffer, int client_i) {
	int i = 0;
	int start = 0;
	char copy[300000];

	bzero(copy, sizeof(copy));

	while (buffer[i]) {
		if (buffer[i] == '\n' || buffer[i] == '\0') {
			// send line
			strcpy(copy, &buffer[start]);
			copy[i - start] = '\0';
			sprintf(message, "client %d: %s\n", client_ids[client_i], copy);
			send_to_all(message, client_socks[client_i]);
			bzero(message, sizeof(message));
			start = i + 1;
		}
		i++;
	}
}

void run() {
	while (1) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(server_fd, &readfds);
		int max_fd = server_fd;

		// add clients to fd set
		for (int i = 0; i < 1000; i++) {
			if (client_socks[i] > 0) {
				FD_SET(client_socks[i], &readfds);
				if (client_socks[i] > max_fd)
					max_fd = client_socks[i];
			}
		}

		select(max_fd + 1,  &readfds, NULL, NULL, NULL);

		// new client
		if (FD_ISSET(server_fd, &readfds)) {
			int new_client = accept(server_fd, ( struct sockaddr *)&address, &addrlen);
			for (int i = 0; i < 1000; i++) {
				if (client_socks[i] == 0) {
					client_socks[i] = new_client;
					client_ids[i] = last_id;
					last_id++;
					sprintf(message, "server: client %d just arrived\n", client_ids[i]);
					send_to_all(message, client_socks[i]);
					bzero(message, sizeof(message));
					break;
				}
			}
		}

		for (int i = 0; i < 1000; i++) {
		// incoming messages from clients
			if (FD_ISSET(client_socks[i], &readfds)) {
				ssize_t bytes = recv(client_socks[i], &buffer, sizeof(buffer), 0);
				if (bytes == 0) {
					// client left
					sprintf(message, "server: client %d just left\n", client_ids[i]);
					send_to_all(message, client_socks[i]);
					bzero(message, sizeof(message));
					close(client_socks[i]);
					client_socks[i] = 0;
				} else {
					// send message to others
					buffer[bytes] = '\0';
					handle_lines(buffer, i);
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		write(2, "Wrong number of arguments\n", 27);
		exit(1);
	}

	int port = atoi(argv[1]);

	setup(port);
	run();
}