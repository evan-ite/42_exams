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

class Server {
public:
    Server(int port, std::string path) : file(path), port(port), server_fd(-1) {
		bzero(clients, sizeof(clients));
		instance = this;
	}

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

    void run() {
        while (true) {
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(server_fd, &readfds);
			int max_fd = server_fd;
			char buffer[10000];
			
			// add clients
			for (int i = 0; i < 1000; i++) {
				if (clients[i] > 0) {
					FD_SET(clients[i], &readfds);
					if (clients[i] > max_fd)
						max_fd = clients[i];
				}
			}

			// select
			select(max_fd + 1, &readfds, NULL, NULL, NULL);

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
						readInput(clients[i], buffer);
					}
				}
			}
		}
    }

    ~Server() {
        if (server_fd >= 0) {
            close(server_fd);
        }
    }

private:
	struct sockaddr_in server_addr;
	socklen_t addrlen;
	std::string file;
    int port;
    int server_fd;
	int clients[1000];
	std::map <std::string, std::string> db;

	static Server *instance;

	bool loadDB() {
		std::ifstream infile(file.c_str());

		if (!infile) return false;
		
		std::string key, value;
		while (infile >> key >> value) {
			db[key] = value;
		}
		return true;
	}

	void saveDB() {
		std::ofstream outfile(file);
		for (std::map<std::string, std::string>::iterator it = db.begin(); it != db.end(); ++it) {
			outfile << it->first << " " << it->second << std::endl;
		}
		close(server_fd);
	}

	static void handlesig(int signal) {
		if (signal == SIGINT) {
			instance->saveDB();
			exit(0);
		}
	}

	void readInput(int fd, std::string buffer) {
		std::istringstream iss(buffer);
		std::string cmd, key, value;
		iss >> cmd;

		int result = 2;
		if (cmd == "POST") {
			iss >> key >> value;
			if (!key.empty() && !value.empty()) {
				db[key] = value;
				result = 0;
			} else {
				result = 1;
			}
		} else if (cmd == "GET") {
			iss >> key;
			if (!key.empty() && db.find(key) != db.end()) {
				std::string response = db[key] + "\n";
				send(fd, response.c_str(), response.size(), 0);
				result = 0;
			} else {
				result = 1;
			}
		} else if (cmd == "DELETE") {
			iss >> key;
			if (!key.empty() && db.erase(key)) {
				result = 0;
			} else {
				result = 1;
			}
		}

		std::string response = std::to_string(result) + "\n";
		send(fd, response.c_str(), response.size(), 0);
	}
};

Server *Server::instance = nullptr;

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
