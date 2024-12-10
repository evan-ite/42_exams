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

typedef struct s_client {
	int fd;
	int id;
	char buf[290000];
} t_client;

int					server_fd;
char				buffer[300000];
char				message[300000];
t_client			clients[1000];
struct sockaddr_in	address;
socklen_t 			addrlen;
int 				last_id = 0;

void setup(int port) {
	bzero(clients, sizeof(clients));
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
		if (clients[i].fd != except_fd && clients[i].fd > 0) {
			send(clients[i].fd, message, strlen(message), 0);
		}
	}
}

void handle_lines(int len, char *buffer, int index) {

	for (int i = 0, j = strlen(clients[index].buf); i < len; i++, j++) {
		clients[index].buf[j] = buffer[i];
		if (clients[index].buf[j] == '\n') {
			clients[index].buf[j] = '\0';
			sprintf(message, "client %d: %s\n", clients[index].id, clients[index].buf);
			send_to_all(message, clients[index].fd);
			bzero(message, sizeof(message));
			bzero(clients[index].buf, sizeof(clients[index].buf));
			j = -1;
		}
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
			if (clients[i].fd > 0) {
				FD_SET(clients[i].fd, &readfds);
				if (clients[i].fd > max_fd)
					max_fd = clients[i].fd;
			}
		}

		select(max_fd + 1,  &readfds, NULL, NULL, NULL);

		// new client
		if (FD_ISSET(server_fd, &readfds)) {
			int new_client = accept(server_fd, ( struct sockaddr *)&address, &addrlen);
			for (int i = 0; i < 1000; i++) {
				if (clients[i].fd == 0) {
					clients[i].fd = new_client;
					clients[i].id = last_id;
					last_id++;
					sprintf(message, "server: client %d just arrived\n", clients[i].id);
					send_to_all(message, clients[i].fd);
					bzero(message, sizeof(message));
					break;
				}
			}
		}

		for (int i = 0; i < 1000; i++) {
		// incoming messages from clients
			if (FD_ISSET(clients[i].fd, &readfds)) {
				ssize_t bytes = recv(clients[i].fd, &buffer, sizeof(buffer), 0);
				if (bytes == 0) {
					// client left
					sprintf(message, "server: client %d just left\n", clients[i].id);
					send_to_all(message, clients[i].fd);
					bzero(message, sizeof(message));
					bzero(clients[i].buf, sizeof(clients[i].buf));
					close(clients[i].fd);
					clients[i].fd = 0;
				} else {
					// send message to others
					buffer[bytes] = '\0';
					handle_lines(bytes, buffer, i);
					bzero(buffer, sizeof(buffer));
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
