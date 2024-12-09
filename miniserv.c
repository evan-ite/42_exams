#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

enum{MAX_CLIENTS = 100,
	BUFFER_SIZE = 40};

int	client_sockets[MAX_CLIENTS];
int client_ids[MAX_CLIENTS];
char buffer[BUFFER_SIZE + 1];
int	last_id = 0;


void send_to_all(char *message, int except_id)
{
	char error_msg[BUFFER_SIZE];

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_sockets[i] && client_ids[i] != except_id) {
			if (send(client_sockets[i], message, strlen(message), 0) < 0) {
				sprintf(error_msg, "Failed to send message to client %d\n", client_ids[i]);
				write(2, error_msg, strlen(error_msg));
			}
		}
	}
}

char *ft_substr(char *str, int start, int len) 
{
	char *newstr = malloc(len - start + 1);
	
	int i = start;
	int j = 0;
	while (i < len) {
		newstr[j] = str[i];
		i++;
		j++;
	}
	newstr[j] = '\0';
	return newstr;
}

void send_lines(int id) 
{
	char message[BUFFER_SIZE];
	
	int start = 0;
	int nl = 0;
	while (buffer[nl] != '\0') {
		if (buffer[nl] == '\n') {
			char *line = ft_substr(buffer, start, nl + 1 - start);
			sprintf(message, "client %d: %s", id, line);
			send_to_all(message, id);
			free(line);
			start = nl + 1;
		}
		nl++;
	}

	int remaining_length = strlen(buffer + start);
    memmove(buffer, buffer + start, remaining_length);
    buffer[remaining_length] = '\0';
}

int main(int argc, char **argv)
{	
	if (argc != 2) {
		write(2, "Wrong number of arguments\n", 27);
		return 1;
	}
	int port = atoi(argv[1]);

	// init client sockets and ids to 0
	bzero(client_sockets, MAX_CLIENTS);
	bzero(client_ids, MAX_CLIENTS);

	// create server socket
	int	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		write(2, "Fatal error\n", 13);
		return 1;
	}

	//create address struct with ip and port
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(port);
	socklen_t addrlen = sizeof(address);

	// bind server to address
	if (bind(server_fd, (const struct sockaddr *)&address, addrlen) < 0) {
		write(2, "Fatal error\n", 13);
		close(server_fd);
		return 1;
	}

	// listen on server socket
	if (listen(server_fd, 5) < 0) {
		write(2, "Fatal error\n", 13);
		close(server_fd);
		return 1;
	}

	while (1) {
		fd_set	readfds;
		FD_ZERO(&readfds);
		FD_SET(server_fd, &readfds);
		int max_fd = server_fd;
		char message[BUFFER_SIZE];

		// add client sockets to readfds
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (client_sockets[i] > 0) {
				FD_SET(client_sockets[i], &readfds);
				if (client_sockets[i] > max_fd)
					max_fd = client_sockets[i];
			}
		}

		// listen to all readfds
		select(max_fd + 1, &readfds, NULL, NULL, NULL);

		// check server for incoming clients
		if (FD_ISSET(server_fd, &readfds)) {
			int new_client = accept(server_fd, (struct sockaddr *)&address, &addrlen);
			if (new_client < 0) {
				write(2, "Error connecting new client\n", 13);
			}

			// add new client to client sockets array, and add id
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (client_sockets[i] == 0) {
					client_sockets[i] = new_client;
					client_ids[i] = last_id;
					sprintf(message, "server: client %d just arrived\n", last_id);
					send_to_all(message, last_id);
					memset(message, 0, sizeof(message));
					last_id++;
					break ;
				}
			}
		}

		// Check clients for incoming messages
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (FD_ISSET(client_sockets[i], &readfds)) {
				int	succes = recv(client_sockets[i], buffer, BUFFER_SIZE, 0);
				if (succes == 0) {
					sprintf(message, "server: client %d just left\n", client_ids[i]);
					send_to_all(message, client_ids[i]);
					memset(message, 0, sizeof(message));
					close(client_sockets[i]);
					client_sockets[i] = 0;
				}
				else {
					buffer[succes + 1] = '\0';
					// sprintf(message, "client %d: %s", client_ids[i], buffer);
					// send_to_all(message, client_ids[i]);
					send_lines(client_ids[i]);
					memset(message, 0, sizeof(message));
					// memset(buffer, 0, sizeof(buffer));
				}
			}
		}
	}

}


/*
Start

// Check for the correct number of arguments
If the number of arguments is not 2
    Print "Wrong number of arguments" to stderr
    Exit with status 1

// Parse the port number from the arguments
port = convert the first argument to an integer

// Initialize variables
MAX_CLIENTS = 100
Create an array client_sockets of size MAX_CLIENTS initialized to 0
Create an array client_ids of size MAX_CLIENTS initialized to 0
buffer_size = 1024
Create a buffer of size buffer_size + 1

// Create a server socket
server_fd = socket(AF_INET, SOCK_STREAM, 0)
If server_fd is 0
    Print "Fatal error" to stderr
    Exit with status 1

// Bind the server socket to 127.0.0.1 and the given port
Create a sockaddr_in struct for address
Set address.sin_family to AF_INET
Set address.sin_addr.s_addr to inet_addr("127.0.0.1")
Set address.sin_port to htons(port)

If bind(server_fd, address, sizeof(address)) < 0
    Print "Fatal error" to stderr
    Close server_fd
    Exit with status 1

// Listen for incoming connections
If listen(server_fd, 3) < 0
    Print "Fatal error" to stderr
    Close server_fd
    Exit with status 1

// Main server loop
While true
    // Clear and set up the file descriptor set
    Initialize fd_set readfds
    FD_ZERO(&readfds)
    FD_SET(server_fd, &readfds)
    max_fd = server_fd

    // Add client sockets to the fd set
    For each client socket in client_sockets
        If the client socket is valid (not 0)
            FD_SET(client socket, &readfds)
            If client socket > max_fd
                max_fd = client socket

    // Use select to wait for activity on any socket
    activity = select(max_fd + 1, &readfds, NULL, NULL, NULL)

    // Check if there is activity on the server socket (new connection)
    If FD_ISSET(server_fd, &readfds)
        Accept the new connection
        If accept fails
            Print "Fatal error" to stderr
            Close server_fd
            Exit with status 1

        // Add the new client to the client_sockets array
        For each client socket in client_sockets
            If the client socket is 0 (empty slot)
                Assign the new connection to this slot
                Assign a unique ID to the new client
                Send "server: client %d just arrived\n" to all other clients
                Break

    // Check each client socket for incoming messages
    For each client socket in client_sockets
        If FD_ISSET(client socket, &readfds)
            Read data from the client socket
            If recv returns 0 (client disconnected)
                Send "server: client %d just left\n" to all other clients
                Close the client socket
                Set the client socket slot to 0
            Else
                Null-terminate the received message
                For each line in the message
                    Prefix the line with "client %d: "
                    Send the prefixed line to all other clients

End
*/
