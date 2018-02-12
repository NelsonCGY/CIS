/*
 * httpServer.cc
 *
 *  Created on: Nov 20, 2017
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

bool debug_mode;
vector<sockaddr_in> user_servers;
vector<sockaddr_in> master_servers;

void *worker(void *);
string readWelcomePage();
string readServicePage(string, string);
string readUserErrorPage();
string readErrorPage();
string readEmailErrorPage();
string readRegisterPage();
string readResetPWDPage();
string readResetErrorPage();
bool validateAccount(string, string);
string createAccount(string, string);
string changePassword(string, string, string);
sockaddr_in string_to_sockaddr(string);
void init_servers(char *);
string getKeyValueAddr();

int main(int argc, char *argv[]) {

	// get config values
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

	// set up server socket
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

	init_servers(argv[optind]);

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
	// read the configuration file
	FILE* file = fopen(filename, "r");
	char buf[256];
	sockaddr_in serv_addr;
	string cur_type = "";
	while (fgets(buf, 256, file) != NULL) {
		string str(buf);
		int idx = str.find('\n');
		if (idx != -1) {
			str = str.substr(0, idx);
		}

		if (str == "USER") {
			cur_type = "USER";
		} else if(str == "MASTER") {
			cur_type = "MASTER";
		} else if(str == "STORAGE1" || str == "STORAGE2" || str == "STORAGE3" || str == "MAIL1" || str == "MAIL2") {
			cur_type = "";
		} else {
			if (cur_type == "USER") {
				user_servers.push_back(string_to_sockaddr(str));
			} else if(cur_type == "MASTER") {
				master_servers.push_back(string_to_sockaddr(str));
			}
		}
	}
}

sockaddr_in string_to_sockaddr(string address) {
	cout << address << endl;
	int idx = address.find(":");
	if (idx == -1) {
		fprintf(stderr, "Invalid configuration file.\n");
		exit(1);
	}
	string ip = address.substr(0, idx);
	int port = stoi(address.substr(idx + 1));
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr));
	return addr;
}

void *worker(void *arg) {
	int comm_fd = *(int*) arg;

	char buf[2048];
	read(comm_fd, buf, 2047);
	printf("%s\n", buf);

	if(!strncmp(buf, "GET /style.css", 14)) {
		int fd_file = open("style.css", O_RDONLY);
		sendfile(comm_fd, fd_file, NULL, 3000);
		close(fd_file);
	} else if(!strncmp(buf, "GET /index.js", 13)) {
		int fd_file = open("index.js", O_RDONLY);
		sendfile(comm_fd, fd_file, NULL, 200);
		close(fd_file);
	} else if(!strncmp(buf, "POST /login", 11)) {
		// string info = strstr(buf, "email=");
		string request(buf);
		int idx = request.find("email=");
		if (idx == -1) {
			string userErrorPage = readUserErrorPage();
			write(comm_fd, userErrorPage.c_str(), userErrorPage.length());
		} else {
			string info = request.substr(idx);
			int idx1 = info.find("=");
			int idx2 = info.find("&");
			string email = info.substr(idx1 + 1, idx2 - idx1 - 1);
			idx1 = info.find("=", idx1 + 1);
			string password = info.substr(idx1 + 1);
			if(validateAccount(email, password)) {
				string servicePage = readServicePage(email, password);
				write(comm_fd, servicePage.c_str(), servicePage.length());
			} else {
				string userErrorPage = readUserErrorPage();
				write(comm_fd, userErrorPage.c_str(), userErrorPage.length());
			}
		}
	} else if(!strncmp(buf, "POST /signup", 12)) {
		// string info = strstr(buf, "email=");
		string request(buf);
		int idx = request.find("email=");
		if (idx == -1) {
			string userErrorPage = readUserErrorPage();
			write(comm_fd, userErrorPage.c_str(), userErrorPage.length());
		} else {
			string info = request.substr(idx);
			int idx1 = info.find("=");
			int idx2 = info.find("&");
			string email = info.substr(idx1 + 1, idx2 - idx1 - 1);
			idx1 = info.find("=", idx1 + 1);
			idx2 = info.find("&", idx2 + 1);
			string password = info.substr(idx1 + 1);
			string page = createAccount(email, password);
			write(comm_fd, page.c_str(), page.length());
		}
	} else if(!strncmp(buf, "GET /register", 13)) {
		string page = readRegisterPage();
		write(comm_fd, page.c_str(), page.length());
	} else if(!strncmp(buf, "GET /changePWD", 14)) {
		string page = readResetPWDPage();
		write(comm_fd, page.c_str(), page.length());
	} else if(!strncmp(buf, "POST /reset", 11)) {
		// string info = strstr(buf, "email=");
		string request(buf);
		int idx = request.find("email=");
		if (idx == -1) {
			string userErrorPage = readUserErrorPage();
			write(comm_fd, userErrorPage.c_str(), userErrorPage.length());
		} else {
			string info = request.substr(idx);
			int idx1 = info.find("=");
			int idx2 = info.find("&");
			string email = info.substr(idx1 + 1, idx2 - idx1 - 1);
			idx1 = info.find("=", idx1 + 1);
			idx2 = info.find("&", idx2 + 1);
			string oldPWD = info.substr(idx1 + 1, idx2 - idx1 - 1);
			idx1 = info.find("=", idx1 + 1);
			idx2 = info.find("&", idx2 + 1);
			string newPWD = info.substr(idx1 + 1);
			string page = changePassword(email, oldPWD, newPWD);
			write(comm_fd, page.c_str(), page.length());
		}
	} else {
		string welcomePage = readWelcomePage();
		write(comm_fd, welcomePage.c_str(), welcomePage.length());
	}

	close(comm_fd);
}

string readRegisterPage() {
	string page = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

	ifstream infile("./register.html");
	string line;
	while (getline(infile, line)) {
		page += line;
	}

	return page;
}

string readResetErrorPage() {
	string page = readResetPWDPage();
	string target = "<form action=\"/reset\" class=\"login-form\" method=\"POST\">";
	int idx = page.find(target);
	page.insert(idx + target.length(), "<b style=\"color:red\">email and old password don't match</b>");
	return page;
}

string readWelcomePage() {
	string page = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

	ifstream infile("./index.html");
	string line;
	while (getline(infile, line)) {
		page += line;
	}

	return page;
}

string readServicePage(string username, string password) {
	string page = "HTTP/1.1 200 OK\r\nSet-Cookie: name=" + username + "\r\nSet-Cookie: password=" + password + "\r\nContent-Type: text/html\r\n\r\n";

	ifstream infile("./service.html");
	string line;
	while (getline(infile, line)) {
		page += line;
	}
	return page;
}

string readUserErrorPage() {
	string page = readWelcomePage();
	string target = "<form action=\"/login\" class=\"login-form\" method=\"POST\">";
	int idx = page.find(target);
	page.insert(idx + target.length(), "<b style=\"color:red\">user doesn't exist</b>");
	return page;
}

string readErrorPage() {
	string page = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
	page += "<html>ERROR</html>";
	return page;
}

string readResetPWDPage() {
	string page = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

	ifstream infile("./resetPWD.html");
	string line;
	while (getline(infile, line)) {
		page += line;
	}

	return page;
}

string readEmailErrorPage() {
	string page = readRegisterPage();
	string target = "<form action=\"/signup\" class=\"login-form\" method=\"POST\">";
	int idx = page.find(target);
	page.insert(idx + target.length(), "<b style=\"color:red\">email already exists</b>");
	return page;
}

string changePassword(string email, string oldPWD, string newPWD) {
	string page;
	char buf[1024];

	if(validateAccount(email, oldPWD)) {
		int keyvalue_fd = socket(PF_INET, SOCK_STREAM, 0);
		if(keyvalue_fd < 0) {
			perror("key value store connection failed");
			exit(1);
		}

		string addr_str = getKeyValueAddr();
		if (addr_str.find("-ERR") == 0) {
			close(keyvalue_fd);
			return readErrorPage();
		}
		sockaddr_in keyvalue_addr = string_to_sockaddr(addr_str);
		connect(keyvalue_fd, (struct sockaddr*)&keyvalue_addr, sizeof(keyvalue_addr));

		// /CPUT r c v1 v2
		string command = "/CPUT " + email + " password " + oldPWD + " " + newPWD + "\r\n";
		write(keyvalue_fd, command.c_str(), command.length());
		memset(buf, 0, sizeof(buf));
		read(keyvalue_fd, buf, 1023);
		string response = buf;
		if(response == "-ERR\r\n") {
			page = readErrorPage();
		} else {
			page = readWelcomePage();
		}
	} else {
		// TODO: old password is not correct
		page = readResetErrorPage();
	}

	return page;
}

bool validateAccount(string email, string password) {
	int masterfd = socket(PF_INET, SOCK_STREAM, 0);
	if(masterfd < 0) {
		perror("master connection failed");
		// exit(1);
		return false;
	}

	connect(masterfd, (struct sockaddr*)&master_servers[0], sizeof(master_servers[0]));

	string command = "/USER\r\n";
	write(masterfd, command.c_str(), command.length());

	char buf[1024];
	read(masterfd, buf, 1023);
	string response = buf;
	// +OKIP 127.0.0.1:5007
	int idx = response.find(" ");

	int keyvalue_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(keyvalue_fd < 0) {
		perror("key value store connection failed");
		// exit(1);
		return false;
	}

	if (idx == -1 || response.find("-ERR") == 0) {
		perror("can't find a node");
		return false;
	}
	sockaddr_in keyvalue_addr = string_to_sockaddr(response.substr(idx + 1));
	connect(keyvalue_fd, (struct sockaddr*)&keyvalue_addr, sizeof(keyvalue_addr));
	command = "/GET " + email + " password\r\n";
	write(keyvalue_fd, command.c_str(), command.length());
	memset(buf, 0, sizeof(buf));
	read(keyvalue_fd, buf, 1023);
	response = buf;
	if(response == "-ERR\r\n") {
		return false;
	} else {
		if(response == password + "\r\n") {
			return true;
		}
	}

	return false;
}

string createAccount(string email, string password) {
	int keyvalue_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(keyvalue_fd < 0) {
		perror("key value store connection failed");
		exit(1);
	}

	string addr_str = getKeyValueAddr();
	if (addr_str.find("-ERR") == 0) {
		close(keyvalue_fd);
		return readErrorPage();
	}
	sockaddr_in keyvalue_addr = string_to_sockaddr(addr_str);
	connect(keyvalue_fd, (struct sockaddr*)&keyvalue_addr, sizeof(keyvalue_addr));
	string command = "/GET " + email + " password\r\n";
	write(keyvalue_fd, command.c_str(), command.length());
	char buf[1024];
	memset(buf, 0, sizeof(buf));
	read(keyvalue_fd, buf, 1023);
	string response = buf;
	string page;
	close(keyvalue_fd);
	if(response == "-ERR\r\n") {
		keyvalue_fd = socket(PF_INET, SOCK_STREAM, 0);
		if(keyvalue_fd < 0) {
			perror("key value store connection failed");
			exit(1);
		}
		addr_str = getKeyValueAddr();
		if (addr_str.find("-ERR") == 0) {
			close(keyvalue_fd);
			return readErrorPage();
		}
		keyvalue_addr = string_to_sockaddr(addr_str);
		connect(keyvalue_fd, (struct sockaddr*)&keyvalue_addr, sizeof(keyvalue_addr));

		command = "/PUT " + email + " password " + password + "\r\n";
		write(keyvalue_fd, command.c_str(), command.length());
		memset(buf, 0, sizeof(buf));
		read(keyvalue_fd, buf, 1023);
		response = buf;
		if(response == "-ERR\r\n") {
			page = readErrorPage();
		} else {
			page = readServicePage(email, password);
		}
	} else {
		page = readEmailErrorPage();
	}
	close(keyvalue_fd);
	return page;
}

string getKeyValueAddr() {
	int masterfd = socket(PF_INET, SOCK_STREAM, 0);
	if(masterfd < 0) {
		perror("master connection failed");
		exit(1);
	}

	connect(masterfd, (struct sockaddr*)&master_servers[0], sizeof(master_servers[0]));

	string command = "/USER\r\n";
	write(masterfd, command.c_str(), command.length());

	char buf[1024];
	read(masterfd, buf, 1023);
	close(masterfd);
	string response = buf;
	int idx = response.find(" ");

	if (idx == -1 || response.find("-ERR") == 0) {
		return response;
	} else {
		return response.substr(idx + 1);
	}
}
