/*
 * SMTPserver.cc
 *
 *  Created on: Nov 19, 2017
 *      Author: cis505
 *
 *      To run server:
 *      ./SMTPserver CONFIGGILE [KVSTORE_PORT_INDEX] [-v] [-p port]
 *      eg.
 *      ./mail serverlist.txt 0 -v -p 4001
 *
 *      Default port2: 4000
 *      KVSTORE_PORT_INDEX = 0 -> use port 5004 to connect kv store
 *                         = 1 -> use port 5005 to connect kv store ...
 *                         = No value assigned -> ask Master for an avaiable port index.
 */

#include "mail.hh"

int port2 = 4000;

int main(int argc, char *argv[]) {

	// Reading input arguments.
	int topt;
	while ((topt = getopt(argc, argv, "p:av")) != -1) {
		switch (topt) {
		case 'v':
			verbose = 1;
			printf("Debug mode ON.\n");
			break;
		case 'p':
			port2 = atoi(optarg);
			if (verbose) std::cout << "optarg " << optarg << std::endl;
			if (verbose) printf("Using port2 %d.\n", port2);
			break;
		case 'a':
			fprintf(stderr, "Yilong Ju / yilongju\n");
			exit(1);
			break;
		case '?':
			if (optopt == 'p') {
				fprintf(stderr, "Please provide a port2 number.\n");
				exit(1);
			}
			fprintf(stderr, "Invalid input.\n");
			exit(1);
			break;
		}
	}

	if (verbose) std::cout << "Using port2 " << port2 << std::endl;
	if (verbose2) std::cout << "argv[argc - 1] " << argv[argc - 1] << std::endl;
	if (isInteger(argv[argc - 1])) {
		serverIdx = std::stoi(argv[argc - 1]);
	}
	if (verbose) std::cout << "Server index " << serverIdx << std::endl;


	std::vector<int> configFilenamePossibleIdx;
	int pIdx = 0;
	int configFilenameIdx = 0;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			configFilenamePossibleIdx.push_back(i);
		}
		if (argv[i][0] == '-' && argv[i][1] == 'p') {
			pIdx = i;
		}
	}

	if (configFilenamePossibleIdx.size() == 1) {
		configFilenameIdx = configFilenamePossibleIdx[0];
	} else {
		for (int idx : configFilenamePossibleIdx) {
			if (idx != pIdx + 1) {
				configFilenameIdx = idx;
				break;
			}
		}
	}

	configFilename = argv[configFilenameIdx];
	if(verbose) std::cout << "configFilename: " << configFilename << std::endl;

	// Parse configuration file
	std::ifstream configFile(configFilename);

	if (!configFile.is_open()) {
		std::cout << "Configuration file does not exist." << std::endl;
		exit(EXIT_FAILURE);
	}

	std::string line;
	std::string currentSetting = "NONE";

	while (std::getline(configFile, line)) {
		if (line.substr(0, 6) == "MASTER") {
			currentSetting = "MASTER";
		} else if (line.substr(0, 7) == "STORAGE") {
			currentSetting = "STORAGE";
		} else if (line.substr(0, 4) == "MAIL") {
			currentSetting = "MAIL";
		} else if (line.substr(0, 4) == "USER") {
			currentSetting = "USER";
		} else {
			if (currentSetting == "MASTER") {
				vec_masterServer.push_back(Str2SockAddr(line));
			} else if (currentSetting == "STORAGE") {
				vec_storageServer.push_back(Str2SockAddr(line));
			} else if (currentSetting == "MAIL") {
				vec_mailServer.push_back(Str2SockAddr(line));
			} else if (currentSetting == "USER") {
				vec_userServer.push_back(Str2SockAddr(line));
			} else {
				std::cout << "Configuration file has bad format." << std::endl;
				exit(EXIT_FAILURE);
			}
		}
	}

    int opt = 1;

    // Creating socket file descriptor
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port2);

    if (bind(listen_fd, (struct sockaddr*)&servaddr,
                                 sizeof(servaddr))<0) {
        perror("Bind failed");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

	// Listen to the socket
    if (listen(listen_fd, 10) < 0) {
        perror("Listen failed");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

	std::cout << std::endl;
	std::thread t[MAX_THREAD_NUM];

	// Create a new socket, and response to client.
	while (new_socket = accept(listen_fd,
			(struct sockaddr*)&clientaddr, (socklen_t*)&clientaddrlen)) {

		if (active_thread_num == MAX_THREAD_NUM) {

			// If the server is full, forbid access from client.
			char server_full[] = "The server is full. Please reconnect later.\n";
			send(new_socket, server_full, strlen(server_full), 0);
			close(new_socket);

		} else {
			t_mutex.lock();
			// Close all zombie thread first.
			for (int i = 0; i < MAX_THREAD_NUM; i++) {
				if (thread_finished[i]) {
					t[close_thread_id].join();
					thread_finished[i] = false;
				}
			}

			// Save all socket_fd.
			socket_vector.push_back(new_socket);

			// Number of active thread ++.
			active_thread_num++;

			// Find the first available thread.
			for (int i = 0; i < MAX_THREAD_NUM; i++) {
				if (thread_occupied[i] == false) {
					thread_id = i;
					thread_occupied[i] = true;
					break;
				}
			}

			// Create a new thread
			t[thread_id] = std::thread(ServerProgram, new_socket);
			t_mutex.unlock();
		}
	}
    return 0;
}

void ServerProgram(int new_socket) {
	static thread_local int thread_socket, tid;
	tid = thread_id;
	thread_socket = new_socket;
	t_mutex.unlock();
	if(verbose) printf("[%d] New Connection from %s\n",
			thread_socket, SockAddr2Str(clientaddr).c_str());

	// ==========================================================
	// Send greeting message
	SendMessage(thread_socket, "220 localhost Mail Transfer Service Ready\n");

	// ==========================================================
	// Creating socket file descriptor for master
	struct sockaddr_in masterAddr;
	socklen_t masterAddrLen = sizeof(masterAddr);

	bzero((char *) &masterAddr, sizeof(masterAddr));
	masterAddr.sin_addr.s_addr = inet_addr(LOCALHOST.c_str());
	masterAddr.sin_family = AF_INET;
	masterAddr.sin_port = vec_masterServer[0].sin_port;
	if(verbose) std::cout << "[1]" << std::endl;

	// ==========================================================
    // Creating socket file descriptor for key-value store
	struct sockaddr_in keyValueAddr;
	socklen_t keyValueAddrLen = sizeof(keyValueAddr);
	bzero((char *) &keyValueAddr, sizeof(keyValueAddr));

	if (serverIdx >= 0) {
		keyValueAddr.sin_addr.s_addr = inet_addr(LOCALHOST.c_str());
		keyValueAddr.sin_family = AF_INET;
		keyValueAddr.sin_port = vec_mailServer[serverIdx].sin_port;
		if(verbose) std::cout << "Using kv port2 " << ntohs(keyValueAddr.sin_port) << std::endl;
	} else {

	}

	// Construct a memory to storage unexecuted segment of command.
	char memory[MAX_MSG_LEN] = {0};
	int memoryUsage = 0;
	int kvSocket = 0;
	signal(SIGINT, sigint);
	std::vector<std::string> mailAddrVec;
	std::vector<std::string> rcptAddrVec;
	std::string lastRcptAddr;
	std::vector<std::string> mboxFilePathVec;
	std::string mailTime;
	std::string mailContent;
	std::string mailSubject;
	FILE* lastMboxFile;

	bool stateHELO = true;
	bool stateInitialized = false;
	bool stateDATA = false;
	bool stateMLFM = false;
	bool stateRCTO = false;
	bool parseCommand = true;
	bool headerWritten = false;
	bool isMainContent = false;

	bool stateGET = false;
	bool statePOST = false;
	bool writeMail = false;
	bool operateMail = false;
	bool reFwMail = false;

	// Send greeting message to key-value store
	bool authenticated = false;
	bool mailSaved = false;
	std::string savedMailTo = "";
	std::string savedMailSubject = "";
	std::string savedMailMainContent = "";

	while (!v_shutdown) {
		// Keep reading message from client.
		char buffer[MAX_MSG_LEN] = {0};
		char t_buffer[MAX_MSG_LEN] = {0};
		char terminator[2] = {0};

		// Read message from client
		if (read(thread_socket, buffer, MAX_MSG_LEN) == -1) {
			fprintf(stderr, "Fail to read client input.\n");
			return;
		}

		if (authenticated) {
			std::string quitCommand = "QUIT\r\n";
			strcpy(buffer, quitCommand.c_str());
		}

		if (strlen(buffer) == 0) {
			continue;
		}

		if(verbose) printf("[%d] B: %s\n", thread_socket, buffer);
		std::string bufferStr = buffer;

		// Add \r\n to the end of POST request
		if (bufferStr.substr(0,4) == "POST") {
			buffer[strlen(buffer)] = '\r';
			buffer[strlen(buffer)] = '\n';
		}

		// Seperate command at "\r\n".
		std::vector<int> termin_locs;
		for (int i = 0; i < strlen(buffer) - 1; i++) {
			if (buffer[i] == '\r' &&
					buffer[i + 1] == '\n') {
				termin_locs.push_back(i);
			}
		}

		if (termin_locs.size() == 0) {

			// If no command terminator is detected,
			// Save the whole buffer to the memory.
			for (int j = 0; j < strlen(buffer); j++) {
				memory[memoryUsage + j] = buffer[j];
			}
			memoryUsage += strlen(buffer);

		} else {

			// If there are terminators, there are also corresponding
			// number of commands.
			int command_num = termin_locs.size();
			for (int i = 0; i < command_num; i++) {


				// For each command, parse and execute it.
				char command[MAX_MSG_LEN] = {0};
				if (i == 0) {

					// The first command should be combined with
					// memory to parse.
					for (int j = 0; j < memoryUsage; j++) {
						command[j] = memory[j];
					}
					for (int j = 0; j < termin_locs[0]; j++) {
						command[memoryUsage + j] = buffer[j];
					}
				} else {

					// The other commands are just in the buffer.
					int command_begin = termin_locs[i - 1] + 2;
					for (int j = command_begin;
							j < termin_locs[i]; j++) {
						command[j - command_begin] = buffer[j];
					}
				}

				// Parse and execute, save the result.
				char keyword[MAX_MSG_LEN] = {0};
				int cmdLen = 0, keyword_fill = 0, charIdx = 0;
				cmdLen = strlen(command);

				// Find the keyword at the beginning of the command, ignoring "\n".
				while (command[charIdx] != ' ' && charIdx < cmdLen) {
					if (command[charIdx] != '\n') {
						keyword[keyword_fill] = command[charIdx];
						keyword_fill++;
					}
					charIdx++;
				}

				if (parseCommand) {

					if (verbose) std::cout << "[command]" << command << std::endl;
					// Parsing the commands
					if (keyword_fill > MAX_KEYWORD_LEN) {
						// If we do not recognize the words.
						char response[MAX_MSG_LEN] = "500 Unrecognized command\r\n";
						send(thread_socket, response, strlen(response), 0);
						if(verbose) printf("[%d] S: %s\n", thread_socket, response);
					} else if (StringMatch(keyword, QUIT)) {

						// If we recognize the words to be QUIT.
						char response[MAX_MSG_LEN] = "221 Service closing transmission channel\r\n";
						send(thread_socket, response, strlen(response), 0);
						if (verbose) printf("[%d] S: %s\n", thread_socket, response);
						if (verbose) printf("[%d] Connection closed.\n", thread_socket);

						t_mutex.lock();
						// Number of active thread num --.
						active_thread_num--;

						// Ready to close those related to this tid.
						close_thread_id = tid;

						// This thread is no longer occupied.
						thread_occupied[close_thread_id] = false;

						// This thread should be joined.
						thread_finished[close_thread_id] = true;
						t_mutex.unlock();

						close(thread_socket);

						break;

					} else if (StringMatch(keyword, HELO)) {

						// Hello message
						if (stateHELO) {
							stateInitialized = true;
							// Check if <domain> is empty
							char domain[MAX_DOMAIN_LEN] = {0};

							for (int i = charIdx + 1; i < cmdLen; i++) {
								domain[i - charIdx - 1] = command[i];
							}

							if (strlen(domain) == 0) {
								SendMessage(thread_socket, "501 Empty domain\r\n");
							} else {
								SendMessage(thread_socket, "250 localhost\r\n");
							}
						} else { // CheckSequence
							SendMessage(thread_socket, "503 Bad sequence of commands\r\n");
						}

					} else if (StringMatch(keyword, MAIL)) {

						// First part of command is MAIL
						for (int i = 4; i < strlen(MAIL_FROM); i++) {
							keyword[i] = command[i];
						}

						if (StringMatch(keyword, MAIL_FROM)) {

							// The full command is MAIL FROM
							if (stateHELO && stateInitialized) {
								stateMLFM = true;
								stateHELO = false;

								// Extract mail address
								char path[MAX_PATH_LEN] = {0};
								int cmdStart = 10;
								for (int i = cmdStart; i < cmdLen; i++) {
									path[i - cmdStart] = command[i];
								}
								std::string path_str = path;
								bool erro = true;

								while ((path_str.find('<') + 1)
										&& (path_str.find('@') + 1)
										&& (path_str.find('>') + 1)) {
									size_t sLoc = path_str.find('<');
									size_t aLoc = path_str.find('@');
									size_t gLoc = path_str.find('>');
									erro = false;

									if (sLoc >= aLoc - 1 || aLoc >= gLoc - 1) {
										erro = true;
										break;
									}
									mailAddrVec.push_back(path_str.substr(1, gLoc - 1));
									path_str.erase(0, gLoc + 1);
								}

								if (verbose) {
									std::cout << "Mail from:" << std::endl;
									for (auto addr : mailAddrVec) {
										std::cout << addr << std::endl;
									}
								}

								if (erro) {
									SendMessage(thread_socket, "501 Bad reverse path\r\n");
								} else {
									SendMessage(thread_socket, "250 OK\r\n");
								}
							} else { // CheckSequence
								SendMessage(thread_socket, "503 Bad sequence of commands\r\n");
							}
						} else { // !StringMatch
							SendMessage(thread_socket, "500 Unrecognized command\r\n");
						}

					} else if (StringMatch(keyword, RCPT)) {

						for (int i = 4; i < strlen(RCPT_TO); i++) {
							keyword[i] = command[i];
						}

						if (StringMatch(keyword, RCPT_TO)) {
							if (stateMLFM) {
								stateRCTO = true;
								// Extract receiver address
								char path[MAX_PATH_LEN] = {0};
								int cmdStart = strlen(RCPT_TO);
								for (int i = cmdStart; i < cmdLen; i++) {
									path[i - cmdStart] = command[i];
								}
								std::string path_str = path;
								std::string invalidUser;
								bool erro = true;
								bool erro2 = false;

								while ((path_str.find('<') + 1)
										&& (path_str.find('@') + 1)
										&& (path_str.find('>') + 1)) {
									size_t sLoc = path_str.find('<');
									size_t aLoc = path_str.find('@');
									size_t gLoc = path_str.find('>');
									std::string rcptDomain;
									std::string rcptName;

									erro = false;
									if (sLoc >= aLoc - 1 || aLoc >= gLoc - 1) {
										erro = true;
										break;
									}

									rcptDomain = path_str.substr(aLoc + 1, gLoc - aLoc - 1);
									rcptName = path_str.substr(sLoc + 1, aLoc - sLoc - 1);

									if (lastRcptAddr != path_str.substr(1, gLoc - 1)) {
										lastRcptAddr = path_str.substr(1, gLoc - 1);
										rcptAddrVec.push_back(lastRcptAddr);
									}
									if (serverIdx < 0) {
										// If no backend server is specified, ask master for it
										keyValueAddr = Str2SockAddr(GetServerAddrByUsername(masterAddr, rcptName, thread_socket));
									}
									std::string response = GetServerResponse(
											keyValueAddr, "/GET " + rcptName + "@" + rcptDomain + " mailNum\r\n", thread_socket);

									if(response.substr(0, 4) == "-ERR") {
										erro = true;
										erro2 = true;
										invalidUser = rcptName + "@" + rcptDomain;
										break;
									}

									path_str.erase(0, gLoc + 1);
								}

								if (erro) {
									if (erro2) {
										SendMessage(thread_socket,
												"550 No such user, " + invalidUser + ", here\r\n");
									} else {
										SendMessage(thread_socket, "501 Bad forward path\r\n");
									}

								} else { // No erro in reverse and forward path
									// Ready to receive the content of mail
									stateDATA = true;
									SendMessage(thread_socket, "250 OK\r\n");
								}
							} else { // CheckSequence
								SendMessage(thread_socket, "503 Bad sequence of commands\r\n");
							}
						}

					} else if (StringMatch(keyword, DATA)) {
						if (strlen(command) > 4) {
							SendMessage(thread_socket, "501 Excessive argument\r\n");
						} else {
							if (stateDATA) {
								// Stop parsing commands, begin to write mail content
								parseCommand = false;
								SendMessage(thread_socket, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
							} else {
								SendMessage(thread_socket, "503 Bad sequence of commands\r\n");
							}
						}

					} else { // StringMatch
						SendMessage(thread_socket, "500 Unrecognized command\r\n");
					}
				} else { // parseCommand = false
					std::string content = command;
					if (verbose) std::cout << "[content]" << content << std::endl;
					// Receive content of email
					if (StringMatch(command, ENDOFMAIL)) {
						if (verbose) std::cout << "Mail Subject: " << mailSubject << std::endl;
						if (verbose) std::cout << "Mail content: " << std::endl << mailContent << std::endl << std::endl;


						for (auto mailAddr: rcptAddrVec) {
							// Save email to each receiver's account
							if (serverIdx < 0) {
								// If no backend server is specified, ask master for it
								AddNewMail(keyValueAddr, masterAddr, mailAddrVec[0],
										thread_socket, mailAddr, mailSubject, mailContent);
							} else {
								AddNewMail(keyValueAddr, masterAddr, mailAddrVec[0],
										thread_socket, mailAddr, mailSubject, mailContent, 0);
							}
						}

						// Reset the state of all variables
						stateHELO = true;
						stateDATA = false;
						stateMLFM = false;
						stateRCTO = false;
						parseCommand = true;
						headerWritten = false;
						isMainContent = false;

						// Clear the addresses and buffers
						mailAddrVec.clear();
						rcptAddrVec.clear();
						lastRcptAddr.clear();
						mboxFilePathVec.clear();
						mailContent.clear();

						SendMessage(thread_socket, "250 OK\r\n");

					} else if (content.substr(0, 9) == "Subject: " && !isMainContent) {

						// Extract subject from mail
						std::vector<std::string> tmp = StrSplit(content, ':');
						if (verbose) std::cout << "[Subject]" << tmp[1] << std::endl;
						mailSubject = tmp[1].substr(1, tmp[1].size() - 1);

					} else if (content.size() == 0 && !isMainContent) {
						// The empty line between header and main content of a mail
						isMainContent = true;

					} else if (isMainContent) {
						// Save mail in a buffer
						mailContent += std::string(command) + "\n";
					}
				}
			}

			// Clear memory, save the rest of text into it.
			bzero(memory, sizeof(memory));
			int memoryBegin;
			if (termin_locs.size() == 0) {
				memoryBegin = 0;
			} else {
				memoryBegin = termin_locs[termin_locs.size() - 1] + 2;
			}

			memoryUsage = strlen(buffer) - memoryBegin;

			for (int j = memoryBegin; j < strlen(buffer); j++) {
				memory[j - memoryBegin] = buffer[j];
			}

			signal(SIGINT, sigint);
		}
	}
}


