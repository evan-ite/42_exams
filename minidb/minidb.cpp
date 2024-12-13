#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <unistd.h>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>

class Server {
	public:
		Server(int port, std::string file): port(port), file(file) {
			server_fd  = -1;
			instance = this;
		}

		~Server() {
			if (server_fd != -1) {
				close(server_fd);
			}
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
			char	buffer[10000];

			bzero(buffer, sizeof(buffer));
			FD_ZERO(&all);
			FD_SET(server_fd, &all);
			max_fd = server_fd;

			while (true) {
				readfds = writefds = all;

				select(max_fd + 1, &readfds, &writefds, NULL, NULL);

				for (int fd = 0; fd <= max_fd; fd++) {
					if (FD_ISSET(fd, &readfds)) {
						if (fd == server_fd) {
							int new_client = accept(server_fd, (struct sockaddr *)&server_addr, &addrlen);
							if (new_client == -1) continue;
							if (new_client > max_fd) max_fd = new_client;
							FD_SET(new_client, &all);
						}
						else {
							ssize_t bytes = recv(fd, &buffer, sizeof(buffer), 0);
							if (bytes <= 0) {
								bzero(buffer, sizeof(buffer));
								close(fd);
								FD_CLR(fd, &all);
							}
							else {
								handle_input(buffer, fd);
								bzero(buffer, sizeof(buffer));
							}
						}
					}
				}
			}
		}

	private:
		int					port;
		std::string			file;

		int					server_fd, max_fd;
		fd_set				all, readfds, writefds;
		std::string			buffer;

		struct sockaddr_in	server_addr;
		socklen_t			addrlen;

		std::map<std::string, std::string>	db;

		static Server*		instance;

		void handle_input(std::string input, int fd) {
			std::istringstream iss(input);
			std::string line, response;

			while(std::getline(iss, line)) {
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
			std::string cmd, key, value, response;

			line_stream >> cmd;

			response = "2\n";

			if (cmd == "POST") {
				line_stream >> key >> value;
				if (!key.empty() && !value.empty()) {
					db[key] = value;
					response = "0\n";
				} else
					response = "1\n";
			} else if (cmd == "GET") {
				line_stream >> key;
				if (!key.empty() && db.find(key) != db.end()) {
					response = db[key] + " 0\n";
				} else
					response = "1\n";
			} else if (cmd == "DELETE") {
				line_stream >> key;
				if (!key.empty() && db.erase(key))
					response = "0\n";
				else
					response = "1\n";
			}

			return response;
		}

		void loadDB() {
			std::ifstream infile(file.c_str());

			if (!infile) return;

			std::string key, value;
			while(infile >> key >> value) {
				db[key] = value;
			}
		}

		void saveDB() {
			std::ofstream outfile(file);

			for(std::map<std::string, std::string>::iterator it = db.begin(); it != db.end(); it++) {
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
};

Server *Server::instance = nullptr;

int main(int argc, char *argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <port> <file>" << std::endl;
		exit(1);
	}

	try {
		int port = atoi(argv[1]);
		std::string file = argv[2];

		Server server(port, file);
		if (server.setup())
			server.run();
	} catch(std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}
