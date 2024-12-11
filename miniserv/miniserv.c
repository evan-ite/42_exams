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
	int id;
	char buf[290000];
} t_client;

struct sockaddr_in	address;
socklen_t 			addrlen;
int					server_fd;
t_client			clients[1024];
fd_set				all, readfds, writefds;
int					last_id = 0, max_fd = 0;
char				recv_buf[300000], send_buf[300000];

void setup(int port) {
	bzero(clients, sizeof(clients));
	bzero(recv_buf, sizeof(recv_buf));
	bzero(send_buf, sizeof(send_buf));

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

	FD_ZERO(&all);
	FD_SET(server_fd, &all);
	max_fd = server_fd;
}

void send_to_all(char *message, int from_fd) {
	for (int fd = 0; fd <= max_fd; fd++) {
		if (FD_ISSET(fd, &writefds) && fd != from_fd && fd != server_fd) {
			send(fd, message, strlen(message), 0);
		}
	}
}

void handle_lines(int bytes, int fd) {
	for (int i = 0, j = strlen(clients[fd].buf); i < bytes; i++, j++) {
		clients[fd].buf[j] = recv_buf[i];
		if (clients[fd].buf[j] == '\n') {
			clients[fd].buf[j] = '\0';
			sprintf(send_buf, "client %d: %s\n", clients[fd].id, clients[fd].buf);
			send_to_all(send_buf, fd);
			bzero(send_buf, sizeof(send_buf));
			bzero(clients[fd].buf, sizeof(clients[fd].buf));
			j = -1;
		}
	}
}

void run() {
	while (1) {
		readfds = writefds = all;

		select(max_fd + 1,  &readfds, &writefds, NULL, NULL);

		// new client
		if (FD_ISSET(server_fd, &readfds)) {
			int new_client = accept(server_fd, ( struct sockaddr *)&address, &addrlen);
			if (new_client == -1) continue;
			clients[new_client].id = last_id++;
			if (new_client > max_fd) max_fd = new_client;
			FD_SET(new_client, &all);
			sprintf(send_buf, "server: client %d just arrived\n", clients[new_client].id);
			send_to_all(send_buf, new_client);
			bzero(send_buf, sizeof(send_buf));
		}

		for (int fd = 0; fd <= max_fd; fd++) {
			// incoming messages from clients
			if (FD_ISSET(fd, &readfds) && fd != server_fd) {
				ssize_t bytes = recv(fd, &recv_buf, sizeof(recv_buf), 0);
				if (bytes == 0) {
					// client left
					sprintf(send_buf, "server: client %d just left\n", clients[fd].id);
					send_to_all(send_buf, fd);
					FD_CLR(fd, &all);
					close(fd);
					bzero(send_buf, sizeof(send_buf));
					bzero(clients[fd].buf, sizeof(clients[fd].buf));
				} else {
					// send message to others
					handle_lines(bytes, fd);
					bzero(recv_buf, sizeof(recv_buf));
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
