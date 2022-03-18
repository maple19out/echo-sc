#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <algorithm>
#include <thread>
#include <mutex>
#include <list>


std::list<int> client_pool;	/* list that saves client socket descriptors */
std::mutex socket_mutex;	/* handle synchronization issue between client sockets */

void usage() {
	printf("syntax: echo-server <port> [-e[-b]]\n");
	printf("sample: echo-server 1234 -e -b\n");
}

struct Param {
	bool echo{false};
	bool broad{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				continue;
			}
			if (strcmp(argv[i], "-b") == 0) {
				broad = true;
				continue;
			}
			port = atoi(argv[i]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	printf("current client number : %ld\n", client_pool.size());
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %ld", res);
			perror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);

		/* echo back received message to the client */
		if (param.echo) {
			socket_mutex.lock();
			res = ::send(sd, buf, res, 0);
			if (res == 0 || res == -1) {
				fprintf(stderr, "send return %ld", res);
				perror(" ");
				break;
			}
			/* broadcast message to clients */
			if (param.broad) {
				for (std::list<int>::iterator iter = client_pool.begin(); iter != client_pool.end(); iter++) {
					if (*iter != sd) {
						res = ::send(*iter, buf, res, 0);
						if (res == 0 || res == -1) {
							fprintf(stderr, "send return %ld", res);
							perror(" ");
							break;
						}
					}
				}
			}
			socket_mutex.unlock();
		}
	}
	/* remove client from client_pool list after disconnection */
	printf("disconnected\n");
	socket_mutex.lock();
	client_pool.erase(std::find(client_pool.begin(), client_pool.end(), sd));
	socket_mutex.unlock();
	printf("current client number : %ld\n", client_pool.size());
	::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	int optval = 1;
	int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(param.port);

	res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res == -1) {
		perror("bind");
		return -1;
	}

	res = listen(sd, 5);
	if (res == -1) {
		perror("listen");
		return -1;
	}

	while (true) {
		struct sockaddr_in client_addr;
		socklen_t len = sizeof(client_addr);
		int client_sd = ::accept(sd, (struct sockaddr *)&client_addr, &len);
		if (client_sd < 0) {
			perror("accept");
			break;
		}

		socket_mutex.lock();
		client_pool.push_back(client_sd);
		socket_mutex.unlock();

		/* use multithreading to handle different clients */
		std::thread* t = new std::thread(recvThread, client_sd);
		t->detach();
	}
	::close(sd);
}
