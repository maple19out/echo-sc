#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
#endif // WIN32
#include <thread>
/*
 * 1. mutex, global variables to manage clients in multi-threading environment (line 17 ~ 23)
 */
#include <mutex>

#define MAX_CLIENT 256

int client_sd[MAX_CLIENT];
int client_cnt = 0;
std::mutex socket_mutex;


#ifdef WIN32
void perror(const char* msg) { fprintf(stderr, "%s %ld\n", msg, GetLastError()); }
#endif // WIN32

void usage() {
	/*
	 * 2. usage modified (line 34 ~ 35)
	 */
	printf("syntax: echo-server <port> [-e[-b]]\n");
	printf("sample: echo-server 1234 -e -b\n");
}

struct Param {
	bool echo{false};
	/*
	 * 3. additional boolean variable to set broadcast mode (line 43)
	 */
	bool broad{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				continue;
			}
			/*
			 * 4. set broad variable to true when option "-b" is given (line 55 ~ 58)
			 */
			if (strcmp(argv[i], "-b") == 0) {
				broad = true;
				continue;
			}
			port = atoi(argv[i++]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	printf("current client number : %d\n", client_cnt);
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
		if (param.echo) {
			res = ::send(sd, buf, res, 0);
			/*
			 * 5. echo to all connected clients if broad variable is set (line 85 ~ 92)
			 */
			if(param.broad) {
				for(int idx=0; idx < MAX_CLIENT; idx++) {
					if(client_sd[idx] == sd || client_sd[idx] == -1)
						continue;
					else
						res = ::send(client_sd[idx], buf, res, 0);
				}
			}

			if (res == 0 || res == -1) {
				fprintf(stderr, "send return %ld", res);
				perror(" ");
				break;
			}
		}
	}
	printf("disconnected\n");
	/*
	 * 6. lock mutex to prevent other clients from changing  global values (line 105 ~ 112)
	 */
	socket_mutex.lock();
	for(int idx=0; idx < MAX_CLIENT; idx++)
		if(client_sd[idx] == sd) {
			client_sd[idx] = -1;
			break;
		}
	client_cnt--;
	socket_mutex.unlock();
	printf("current client number : %d\n", client_cnt);
	::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	WSAStartup(0x0202, &wsaData);
#endif // WIN32

	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	int res;
#ifdef __linux__
	int optval = 1;
	res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}
#endif // __linux

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(param.port);

	ssize_t res2 = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res2 == -1) {
		perror("bind");
		return -1;
	}

	res = listen(sd, 5);
	if (res == -1) {
		perror("listen");
		return -1;
	}
	/*
	 * 7. initializing client_sd array to -1 (line 163 ~ 164)
	 */
	for(int i=0; i<MAX_CLIENT; i++)
		client_sd[i] = -1;

	while (true) {
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);
		int cli_sd = ::accept(sd, (struct sockaddr *)&cli_addr, &len);
		if (cli_sd == -1) {
			perror("accept");
			break;
		}

		/*
		 * 8. add client number and save socket descriptor to client_sd (line 179 ~ 187)
		 */

		socket_mutex.lock();
		int idx = 0;
		for(; idx<MAX_CLIENT; idx++)
			if(client_sd[idx] == -1)
				break;

		client_sd[idx] = cli_sd;
		client_cnt++;
		socket_mutex.unlock();

		std::thread* t = new std::thread(recvThread, cli_sd);
		t->detach();
	}
	::close(sd);
}
