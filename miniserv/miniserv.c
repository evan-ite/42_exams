#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct s_client {
	int id;
	char buf[290000];
} t_client;

fd_set		all, readfds, writefds;
t_client	clients[1024];
char		recv_buf[300000], send_buf[300000];
int			last_id = 0, max_fd, server_fd;

struct sockaddr_in	servaddr;
socklen_t			len;

void setup(int port)
{
	bzero(clients, sizeof(clients));
	bzero(recv_buf, sizeof(recv_buf));
	bzero(send_buf, sizeof(send_buf));

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		write(2, "Fatal error\n", 13);
		exit(1);
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(port);
	len = sizeof(servaddr);

	if (bind(server_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
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

void send_all(char *msg, int from_fd)
{
	for (int fd = 0; fd <= max_fd; fd++) {
		if (FD_ISSET(fd, &writefds) && fd != from_fd && fd != server_fd) {
			send(fd, msg, strlen(msg), 0);
		}
	}
}

void handle_input(int bytes, int fd)
{
	for (int i = 0, j = strlen(clients[fd].buf); i < bytes; i++, j++) {
		clients[fd].buf[j] = recv_buf[i];
		if (clients[fd].buf[j] == '\n') {
			clients[fd].buf[j] = '\0';
			sprintf(send_buf, "client %d: %s\n", clients[fd].id, clients[fd].buf);
			send_all(send_buf, fd);
			bzero(clients[fd].buf, sizeof(clients[fd].buf));
			bzero(send_buf, sizeof(send_buf));
			j = -1;
		}
	}
}

void run()
{
	while (1) {
		readfds = writefds = all;

		select(max_fd + 1, &readfds, &writefds, NULL, NULL);

		for (int fd = 0; fd <= max_fd; fd++) {
			if (FD_ISSET(fd, &readfds)) {
				if (fd == server_fd) {
					int new_client = accept(server_fd, (struct sockaddr *)&servaddr, &len);
					if (new_client == -1) continue;
					if (new_client > max_fd) max_fd = new_client;
					clients[new_client].id = last_id++;
					FD_SET(new_client, &all);
					FD_SET(new_client, &writefds);
					sprintf(send_buf, "server: client %d just arrived\n", clients[new_client].id);
					send_all(send_buf, new_client);
					bzero(send_buf, sizeof(send_buf));
				}
				else {
					ssize_t bytes = recv(fd, &recv_buf, sizeof(recv_buf), 0);

					if (bytes <= 0) {
						sprintf(send_buf, "server: client %d just left\n", clients[fd].id);
						send_all(send_buf, fd);
						close(fd);
						FD_CLR(fd, &all);
						FD_CLR(fd, &writefds);
						FD_CLR(fd, &readfds);
						bzero(send_buf, sizeof(send_buf));
						bzero(clients[fd].buf, sizeof(clients[fd].buf));
					}
					else {
						handle_input(bytes, fd);
						bzero(recv_buf, sizeof(recv_buf));
					}
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		write(2, "Wrong number of arguments\n", 27);
		exit(1);
	}


	int port = atoi(argv[1]);
	setup(port);

	run();
}
