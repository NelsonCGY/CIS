/*
 * loadbalancer.cc
 *
 *  Created on: Dec 3, 2017
 *      Author: cis505
 */

#include <cctype>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/sendfile.h>
#include <vector>

using namespace std;

typedef struct {
	sockaddr_in addr;
	int port;
} mySocket;

bool debug_mode;
int front_idx = 0;
int mail_idx;
int drive_idx;
pthread_mutex_t lock;
vector<mySocket> front_servers;
vector<mySocket> mail_servers;
vector<mySocket> drive_servers;


mySocket string_to_sockaddr(string);
void *worker(void *);
void init_servers(char *);
int getPort(string);
string parse_cookie(string request);
vector<string> split(string str, string delimiter);

int main(int argc, char *argv[]) {
	int c = 0;
	int port = 10000;
	while ((c = getopt(argc, argv, "p:v::")) != -1) {
		switch (c) {
		case '?':
			fprintf(stderr, "Author: Wen Zhong\nSeas Login: wenzhong\n");
			return -1;
		case 'p':
			if (optarg == NULL || !isdigit(optarg[0])) {
				fprintf(stderr, "Author: Wen Zhong\nSeas Login: wenzhong\n");
				return -1;
			}
			port = atoi(optarg);
			break;
		case 'v':
			debug_mode = true;
			break;
		}
	}

	init_servers(argv[optind]);

	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1) {
		perror("Socket");
		exit(1);
	}

	int temp;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(int))
			== -1) {
		perror("Setsockopt");
		exit(1);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(port);
	bind(listen_fd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	listen(listen_fd, 10);

	pthread_mutex_init(&lock, NULL);
	while(true) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);

		int* fd = (int*) malloc(sizeof(int));
		*fd = accept(listen_fd, (struct sockaddr*) &clientaddr, &clientaddrlen);

		pthread_t thread;

		int r = pthread_create(&thread, NULL, worker, fd);
		if(r == 0) {
			cout << "success" << endl;
		}
	}
}



void init_servers(char *filename) {
	FILE* file = fopen(filename, "r");
	enum type{storage, mail, user};
	type cur_type = user;
	char buf[256];
	while (fgets(buf, 256, file) != NULL) {
		string str(buf);
		int idx = str.find('\n');
		if (idx != -1) {
			str = str.substr(0, idx);
		}

		if (str == "STORAGE") {
			cur_type = storage;
		} else if (str == "MAIL") {
			cur_type = mail;
		} else if (str == "USER") {
			cur_type = user;
		} else {
			if (cur_type == storage) {
				drive_servers.push_back(string_to_sockaddr(str));
			} else if (cur_type == mail) {
				mail_servers.push_back(string_to_sockaddr(str));
			} else if(cur_type == user) {
				front_servers.push_back(string_to_sockaddr(str));
			}
		}
	}
}

mySocket string_to_sockaddr(string address) {
	cout << address << endl;
	int idx = address.find(":");
	if (idx == -1) {
		fprintf(stderr, "Invalid configuration file.\n");
		exit(1);
	}
	mySocket sock;
	string ip = address.substr(0, idx);
	int port = stoi(address.substr(idx + 1));
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr));
	sock.addr = addr;
	sock.port = port;
	return sock;
}

void *worker(void *arg) {
	int comm_fd = *(int*) arg;

	char buf[2048];
	read(comm_fd, buf, 2047);
	printf("%s\n", buf);

	int port = getPort(buf);

	mySocket sock;
	if(port == 10000) {
		cout << "Port: " << port << endl;
		pthread_mutex_lock(&lock);
		sock = front_servers[front_idx % front_servers.size()];
		front_idx++;
		pthread_mutex_unlock(&lock);
	} else if(port == 9000) {
		cout << "Port: " << port << endl;
		pthread_mutex_lock(&lock);
		sock = mail_servers[mail_idx % mail_servers.size()];
		mail_idx++;
		pthread_mutex_unlock(&lock);
	} else if(port == 8000) {
		cout << "Port: " << port << endl;
		pthread_mutex_lock(&lock);
		sock = drive_servers[drive_idx % drive_servers.size()];
		drive_idx++;
		pthread_mutex_unlock(&lock);
	}
	cout << "current port is: " <<  sock.port << endl;
	string request(buf);
	string response = "HTTP/1.1 301 Moved Permanently\r\nLocation:http://localhost:" + to_string(sock.port);
	static const string cookie = "Cookie: name=";
	int start = request.find(cookie);
	int end = request.find(";", start + cookie.length());
	string username = "";
	if (start != -1 && end != -1) {
		username = request.substr(start + cookie.length(), end - start - cookie.length());
		cout << "Username is " << username << endl;
	}
	response += "/?username=" + username + "\r\n\r\n";
	cout << response;
	write(comm_fd, response.c_str(), response.length());
	close(comm_fd);
}

int getPort(string request) {
	string key = "Host: localhost:";
	int index = request.find(key);
	int port = 0;
	index = index + key.length();
	while(index < request.length() && request[index] >= '0' && request[index] <= '9') {
		port = port * 10 + (request[index] - '0');
		index++;
	}
	return port;
}
