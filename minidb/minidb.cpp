#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <csignal>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <map>

/**
 * @class Server
 * @brief A simple server class to handle client connections and a key-value database.
 */
class Server {
public:
	/**
	 * @brief Constructor to initialize the server with a port and file path.
	 * @param port The port number to listen on.
	 * @param path The file path for the database.
	 */
	Server(int port, std::string path) : file(path), port(port), server_fd(-1) {
		instance = this;
	}

	/**
	 * @brief Sets up the server by creating a socket, binding, and listening.
	 * @return True if setup is successful, false otherwise.
	 */
	bool setup() {
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd < 0) {
			std::cerr << "Failed to create socket" << std::endl;
			return false;
		}

		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.sin_port = htons(port);
		addrlen = sizeof(server_addr);

		if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
			std::cerr << "Bind failed" << std::endl;
			close(server_fd);
			return false;
		}

		if (listen(server_fd, 5) < 0) {
			std::cerr << "Listen failed" << std::endl;
			close(server_fd);
			return false;
		}

		loadDB();
		signal(SIGINT, handlesig);

		std::cout << "Server setup complete. Listening on port " << port << std::endl;
		return true;
	}

	/**
	 * @brief Runs the server to accept and handle client connections.
	 */
	void run() {
		int clients[1000];
		bzero(clients, sizeof(clients));

		while (true) {
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(server_fd, &readfds);
			int max_fd = server_fd;
			char buffer[10000];

			// Add clients to fd_set
			for (int i = 0; i < 1000; i++) {
				if (clients[i] > 0) {
					FD_SET(clients[i], &readfds);
					if (clients[i] > max_fd)
						max_fd = clients[i];
				}
			}

			select(max_fd + 1, &readfds, NULL, NULL, NULL);

			// New client connecting
			if (FD_ISSET(server_fd, &readfds)) {
				int new_client = accept(server_fd, (struct sockaddr *)&server_addr, &addrlen);
				for (int i = 0; i < 1000; i++) {
					if (clients[i] < 1) {
						clients[i] = new_client;
						break;
					}
				}
			}

			for (int i = 0; i < 1000; i++) {
				// Handle input from connected clients
				if (FD_ISSET(clients[i], &readfds)) {
					ssize_t bytes = recv(clients[i], &buffer, sizeof(buffer), 0);
					if (bytes == 0) {
						close(clients[i]);
						clients[i] = 0;
					}
					else if (bytes < 0) {
						std::cerr << "Error receiving from client: " << clients[i] << std::endl;
						close(clients[i]);
						clients[i] = 0;
					}
					else {
						buffer[bytes] = '\0';
						handleInput(clients[i], buffer);
					}
				}
			}
		}
	}

	/**
	 * @brief Destructor to close the server socket.
	 */
	~Server() {
		if (server_fd >= 0) {
			close(server_fd);
		}
	}

private:
	struct sockaddr_in					server_addr; ///< Server address structure.
	socklen_t							addrlen; ///< Length of the server address.
	std::string							file; ///< File path for the database.
	int									port; ///< Port number to listen on.
	int									server_fd; ///< Server file descriptor.
	std::map <std::string, std::string>	db; ///< Key-value database.
	static Server*						instance; ///< Static instance of the server for signal handling.

	/**
	 * @brief Loads the database from the file.
	 * @return True if the database is loaded successfully, false otherwise.
	 */
	bool loadDB() {
		std::ifstream infile(file.c_str());

		if (!infile) return false;

		std::string key, value;
		while (infile >> key >> value) {
			db[key] = value;
		}
		return true;
	}

	/**
	 * @brief Saves the database to the file.
	 */
	void saveDB() {
		std::ofstream outfile(file);
		for (std::map<std::string, std::string>::iterator it = db.begin(); it != db.end(); ++it) {
			outfile << it->first << " " << it->second << std::endl;
		}
		close(server_fd);
	}

	/**
	 * @brief Signal handler to save the database on SIGINT.
	 * @param signal The signal number.
	 */
	static void handlesig(int signal) {
		if (signal == SIGINT) {
			instance->saveDB();
			exit(0);
		}
	}

	/**
	 * @brief Reads and processes input from a client.
	 * @param fd The file descriptor of the client.
	 * @param buffer The input buffer from the client.
	 */
	void handleInput(int fd, std::string buffer) {
		std::istringstream iss(buffer);
		std::string line;

		std::string response = std::to_string(2) + "\n";

		while (std::getline(iss, line)) {
			if (!line.empty())
				response = processCommand(line);
		}

		if (!iss.eof()) {
			std::string remainder;
			std::getline(iss, remainder, '\0');
			if (!remainder.empty())
				response = processCommand(remainder);
		}

		send(fd, response.c_str(), response.size(), 0);
	}

	std::string processCommand(std::string line) {
		std::istringstream line_stream(line);
		std::string cmd, key, value;
		line_stream >> cmd;

		std::string response = std::to_string(2) + "\n";

		if (cmd == "POST") {
			line_stream >> key >> value;
			if (!key.empty() && !value.empty()) {
				db[key] = value;
				response = std::to_string(0) + "\n";
			} else {
				response = std::to_string(1) + "\n";
			}
		} else if (cmd == "GET") {
			line_stream >> key;
			if (!key.empty() && db.find(key) != db.end()) {
				response = std::to_string(0) + " " + db[key] + "\n";
			} else {
				response = std::to_string(1) + "\n";
			}
		} else if (cmd == "DELETE") {
			line_stream >> key;
			if (!key.empty() && db.erase(key)) {
				response = std::to_string(0) + "\n";
			} else {
				response = std::to_string(1) + "\n";
			}
		}

		return response;
	}
};

Server *Server::instance = nullptr;

/**
 * @brief Main function to start the server.
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments.
 * @return 0 on success, 1 on failure.
 */
int main(int argc, char *argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <port> <path>" << std::endl;
		return 1;
	}

	int port = atoi(argv[1]);
	std::string path = argv[2];
	Server server(port, path);

	try {
		if (!server.setup()) {
			std::cerr << "Server setup failed" << std::endl;
			return 1;
		}
		server.run();
	} catch (std::exception &e) {
		std::cout << "Error: " << e.what() << std::endl;
	}

	return 0;
}
