/*
 * mail.cc
 *
 *  Created on: Nov 19, 2017
 *      Author: cis505
 *
 *      To run server:
 *      ./mail CONFIGGILE [KVSTORE_PORT_INDEX] [-v] [-p PORT]
 *      eg.
 *      ./mail backend.txt 0 -v -p 9000
 *
 *      Default PORT: 9000
 *      KVSTORE_PORT_INDEX = 0 -> use port 5004 to connect kv store
 *                         = 1 -> use port 5005 to connect kv store ...
 *                         = No value assigned -> ask Master for an avaiable port index.
 */

#include "mail.hh"


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
			port = atoi(optarg);
			if (verbose) std::cout << "optarg " << optarg << std::endl;
			if (verbose) printf("Using port %d.\n", port);
			break;
		case 'a':
			fprintf(stderr, "Yilong Ju / yilongju\n");
			exit(1);
			break;
		case '?':
			if (optopt == 'p') {
				fprintf(stderr, "Please provide a port number.\n");
				exit(1);
			}
			fprintf(stderr, "Invalid input.\n");
			exit(1);
			break;
		}
	}

	if (verbose) std::cout << "Using port " << port << std::endl;
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
    servaddr.sin_port = htons(port);

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
		if(verbose) std::cout << "Using kv port " << ntohs(keyValueAddr.sin_port) << std::endl;
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
	FILE* lastMboxFile;

	bool stateHELO = true;
	bool stateInitialized = false;
	bool stateDATA = false;
	bool stateMLFM = false;
	bool stateRCTO = false;
	bool parseCommand = true;
	bool headerWritten = false;

	bool stateGET = false;
	bool statePOST = false;
	bool writeMail = false;
	bool operateMail = false;
	bool reFwMail = false;

	if (testing) {
		query_MX_record("seas.upenn.edu");
		query_MX_record("gmail.com");
		query_MX_record("smtp.gmail.com");

		currentUser = "Bob@penncloud.com";
		currentUserMailAddr = "Bob@penncloud.com";

		// =========== Test ==== add some users and emails
		GetServerResponse(keyValueAddr, "/GET 1 2\r\n", thread_socket);
		if(verbose) std::cout << "[T11]" << std::endl;
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com mail1\r\n", thread_socket);
		if(verbose) std::cout << "[T12]" << std::endl;
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com mail1 " + encode_base64("Mon Nov 27 10:25:21 2017"
				"|acc@penncloud.com>SUB1\thaha1") + "\r\n", thread_socket);
		if(verbose) std::cout << "[T13]" << std::endl;
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com mail2\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com mail2 " + encode_base64("Mon Nov 27 11:25:21 2017"
				"|yaa@penncloud.com>SUB2\thaha2") + "\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com mailNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com mailNum 2\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com mailIdxMax\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com mailIdxMax 2\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com sentNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com sentNum 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com sentIdxMax\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com sentIdxMax 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com trashNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com trashNum 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com trashIdxMax\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com trashIdxMax 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Amy@penncloud.com mailAddr\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Amy@penncloud.com mailAddr Amy@penncloud.com\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mail1\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com mail1 " + encode_base64("Mon Nov 27 12:25:21 2017|pod@penncloud.com>SUBA\thahaA") + "\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/GET Bob@penncloud.com mail1\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mail2\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com mail2 " + encode_base64("Mon Nov 27 02:25:21 2017|"
				"axx@penncloud.com>SUBB\tKeffiyeh blog actually fashion axe vegan, irony biodiesel. Cold-pressed h"
				"oodie chillwave put a bird on it aesthetic, bitters brunch meggings vegan iPhone. Dreamcatche"
				"r vegan scenester mlkshk. Ethical master cleanse Bushwick, occupy Thundercats banjo cliche en"
				"nui farm-to-table mlkshk fanny pack gluten-free. Marfa butcher vegan quinoa, bicycle rights d"
				"isrupt tofu scenester chillwave 3 wolf moon asymmetrical taxidermy pour-over. Quinoa tote bag"
				" fashion axe, Godard disrupt migas church-key tofu blog locavore. Thundercats cronut polaroid"
				" Neutra tousled, meh food truck selfies narwhal American Apparel") + "\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mail3\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com mail3 " + encode_base64("Mon Nov 27 03:25:21 2017"
				"|xzz@penncloud.com>SUBC\tRaw denim McSweeney's bicycle rights, iPhone") + "\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mail4\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com sent1\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com trash1\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mailNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mailNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com mailNum 3\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/GET Bob@penncloud.com mailNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mailIdxMax\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com mailIdxMax 3\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com sentNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com sentNum 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com sentIdxMax\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com sentIdxMax 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com trashNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com trashNum 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com trashIdxMax\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com trashIdxMax 0\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/DELETE Bob@penncloud.com mailAddr\r\n", thread_socket);
		GetServerResponse(keyValueAddr, "/PUT Bob@penncloud.com mailAddr Bob@penncloud.com\r\n", thread_socket);
		// ==========================================================
		testing = false;
	}
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
		close_thread_id = tid;

		// Read message from client
		if (read(thread_socket, buffer, MAX_MSG_LEN) == -1) {
			fprintf(stderr, "Fail to read client input.\n");
			return;
		}

		if (authenticated && 0) {
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
					std::string commandStr = command;
					// Parsing the commands line by line
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

					} else if (StringMatch(keyword, SCHECK)) {

						// Response to master node's check message
						if(verbose) printf("[Master, %d]%s\n", thread_socket, command);
						std::string response = "+GOOD\r\n";
						send(thread_socket, response.c_str(), response.size(), 0);
						if(verbose) printf("[MAIL, %d]%s", thread_socket, response.c_str());

					} else if (StringMatch(keyword, SUSER)) {

						// Save user if it is passed by "/USER"
						std::vector<std::string> vec_tmp = StrSplit(command, ' ');
						currentUser = url_decode(vec_tmp[1]);
						currentPw = vec_tmp[2];
						currentUserMailAddr = url_decode(vec_tmp[1]);

						if(verbose) {
							std::cout << "Current user: " << currentUser << std::endl;
							std::cout << "Current pw: " << currentPw << std::endl;
							std::cout << "Current currentUserMailAddr: " << currentUserMailAddr << std::endl;
						}

						if(verbose) std::cout << "[masterAddr]" << SockAddr2Str(masterAddr) << std::endl;

						// Get key-value store port for current user
						if (serverIdx < 0) {
							keyValueAddr = Str2SockAddr(
									GetServerAddrByUsername(masterAddr, currentUser, thread_socket));
							if (verbose) std::cout << "[keyValueAddr]" << SockAddr2Str(keyValueAddr) << std::endl;
						}

						authenticated = true;
						SendMessage(thread_socket, "+OK\r\n");

					} else if (StringMatch(keyword, GET)) {

						// Response to GET request
						if (stateGET) {
							SendMessage(thread_socket, "-ERR\r\n");

						} else {

							if(verbose) std::cout << "Client GET" << std::endl;
							stateGET = true;

							// Extract request parameters
							std::vector<std::string> paramsReq = StrSplit(command, ' ');

							reqType = "";
							reqPage = "";
							reqProto = "";

							if (paramsReq.size() >= 1) reqType = paramsReq[0];
							if (paramsReq.size() >= 2) reqPage = paramsReq[1];
							if (paramsReq.size() >= 3) reqProto = paramsReq[2];

							if (verbose) std::cout << "[reqType]" << reqType << std::endl;
							if (verbose) std::cout << "[reqPage]" << reqPage << std::endl;
							if (verbose) std::cout << "[reqProto]" << reqProto << std::endl;

						}

					} else if (commandStr.substr(0, 7) == "Cookie:") {

						// Set cookie if given
						std::vector<std::string> tmp_split = StrSplit(command, '=');
						std::vector<std::string> tmp_split2 = StrSplit(tmp_split[1], ';');
						currentUser = url_decode(tmp_split2[0]);

						if (verbose) std::cout << "[currentUser by cookie]" << currentUser << std::endl;

						if (serverIdx < 0) {
							keyValueAddr = Str2SockAddr(
									GetServerAddrByUsername(masterAddr, currentUser, thread_socket));
							if (verbose) std::cout << "[keyValueAddr]" << SockAddr2Str(keyValueAddr) << std::endl;
						}

						authenticated = true;

					} else if (commandStr.substr(0, 9) == "Location:") {

						// If redirected, get current user from cookie
						if (reqPage.substr(0, 10) == "?username=") {
							std::vector<std::string> tmp = StrSplit(reqPage, '=');
							currentUser = url_decode(tmp[1]);
							// currentUserMailAddr = GetServerResponse(keyValueAddr,
							//		"/GET " + currentUser + " mailAddr\r\n", thread_socket);
							currentUserMailAddr = url_decode(tmp[1]);
							if (verbose) std::cout << "[currentUser]" << currentUser << std::endl;
							if (verbose) std::cout << "[masterAddr]" << SockAddr2Str(masterAddr) << std::endl;
							if (!testing) {
								keyValueAddr = Str2SockAddr(
										GetServerAddrByUsername(masterAddr, currentUser, thread_socket));
								if (verbose) std::cout << "[keyValueAddr]" << SockAddr2Str(keyValueAddr) << std::endl;
							}

							authenticated = true;
						}

						std::string content = GetIndexPageContent(keyValueAddr, currentUser, thread_socket, port);
						std::string response = reqProto + OK_200;
						response += TEXT_HTML;
						response += SetCookieUsername(currentUser);
						response += CONTENT_LEN;
						response += std::to_string(content.size());
						response += CONTENT_BEGIN + content;

						SendMessage(thread_socket, response);

					} else if (strlen(keyword) == 0) {

						// If there is an empty, execute following commands based on current state
						if (reqType.size() != 0 && reqPage.size() != 0 && reqProto.size() != 0) {

							// If non empty request, find the second / of the page, decide what it is
							std::vector<std::string> tmp_split = StrSplit(reqPage, '/');
							if (tmp_split.empty()) {
								reqPage = "";
							} else {
								reqPage = tmp_split[tmp_split.size() - 1];
							}
							if (verbose) std::cout << "[reqPage2]" << reqPage << std::endl;

							if (stateGET) {

								if (verbose) std::cout << "[stateGET]" << stateGET << std::endl;
								if (reqPage == "mail_index.html" || reqPage == "" || reqPage == "/" ||
										reqPage.substr(0, 10) == "?username=") {

									// If the homepage is requested
									std::string response = reqProto + OK_200;
									response += TEXT_HTML;
									if (reqPage.substr(0, 10) == "?username=") {

										// If cookie is given
										std::vector<std::string> tmp = StrSplit(reqPage, '=');
										currentUser = url_decode(tmp[1]);
										currentUserMailAddr = url_decode(tmp[1]);
										if (verbose) std::cout << "[currentUser]" << currentUser << std::endl;
										if (verbose) std::cout << "[masterAddr]" << SockAddr2Str(masterAddr) << std::endl;
										if (serverIdx < 0) {
											keyValueAddr = Str2SockAddr(
													GetServerAddrByUsername(masterAddr, currentUser, thread_socket));
											if (verbose) std::cout << "[keyValueAddr]" << SockAddr2Str(keyValueAddr) << std::endl;
										}

										authenticated = true;
										response += SetCookieUsername(currentUser);
									}

									// Send back homepage
									std::string content = GetIndexPageContent(
											keyValueAddr, currentUser, thread_socket, port);

									response += CONTENT_LEN;
									response += std::to_string(content.size());
									response += CONTENT_BEGIN + content;

									SendMessage(thread_socket, response);

								} else if (reqPage == "?username=" && currentUser == "") {

									// If cookie is empty and no user is logged in
									std::string content = "<!DOCTYPE html><html>"
											"<a href='http://localhost:10000'>"
											"<b>Please Login.</b></a</html>";
									std::string response = reqProto + OK_200;
									response += TEXT_HTML;
									response += CONTENT_LEN;
									response += std::to_string(content.size());
									response += CONTENT_BEGIN + content;

								} else if (reqPage.find("mailbox.css") != std::string::npos) {

									std::string response = reqProto + OK_200;
									response += TEXT_CSS + CONTENT_LEN;
									response += std::to_string(MAILBOX_CSS.size());
									response += CONTENT_BEGIN + MAILBOX_CSS;
									SendMessage(thread_socket, response);

								} else if (reqPage.substr(0, 9) == "read.html") {

									// Page for reading mails
									std::vector<std::string> tmpVec_str = StrSplit(reqPage, '&');
									std::string mailID = tmpVec_str[1];
									if (verbose) std::cout << "[mailID]" << mailID << std::endl;

									std::string reqKV = "/GET " + currentUser +
											" " + mailID + "\r\n";
									std::string mailStr = decode_base64(GetServerResponse(keyValueAddr, reqKV, thread_socket));
									struct Mail mail = ParseMail(mailStr, currentUserMailAddr);

									std::string content = GetSidebar(keyValueAddr,
											currentUser, thread_socket) +
											READ_BEFORE_SUBJECT + mail.mailSubject +
											READ_BEFORE_FROM + "From: " + mail.mailFrom +
											", To: " + mail.mailTo +
											READ_BEFORE_TIME + mail.mailTime +
											READ_BEFORE_MAINCONTENT +
											ParagraphMailContent(mail.mailMainContent) +
											READ_END;

									std::string response = reqProto + OK_200;
									response += TEXT_HTML + CONTENT_LEN;
									response += std::to_string(content.size());
									response += CONTENT_BEGIN + content;

									SendMessage(thread_socket, response);

								} else if (reqPage.substr(0, 12) == "compose.html") {

									// Page for composing mails
									std::string content = GetSidebar(keyValueAddr,
											currentUser, thread_socket) +
											COMPOSE_BEFORE_FROM +
											currentUserMailAddr;

									if (mailSaved) {

										// Recover from error page
										content += GetComposeAfterForm(savedMailTo,
												savedMailSubject,
												savedMailMainContent);

										if(verbose) std::cout << "==== Mail loaded ====" << std::endl;
										mailSaved = false;
									} else {
										content += GetComposeAfterForm();
									}

									std::string response = reqProto + OK_200;
									response += TEXT_HTML + CONTENT_LEN;
									response += std::to_string(content.size());
									response += CONTENT_BEGIN + content;

									SendMessage(thread_socket, response);

								} else {
									std::string response = reqProto + ERR_404;
									SendMessage(thread_socket, response);
								}
							}

							if (statePOST) {

								// If POST request is received
								if (reqPage == "mail_index.html" || reqPage == "") {
									// homepage does not send POST request in this implementation

								} else if (reqPage.substr(0, 9) == "read.html" && !reFwMail) {

									// POST request in the page for reading mails will be one of Reply, Forward or Delete
									// Begin to perform those action
									if(verbose) std::cout << "-------------------------- read.html --------------------------" << std::endl;
									operateMail = true;
									if (reFwMail) {
										// If user replies or forwards mail, switch into write mail state
										writeMail = true;
									}

								} else if (reqPage.substr(0, 12) == "compose.html" || reFwMail) {

									// Prepare for sending mail
									if(verbose) std::cout << "-------------------------- compose.html --------------------------" << std::endl;
									writeMail = true;
									reFwMail = false;

								} else {
									std::string response = reqProto + ERR_404;
									SendMessage(thread_socket, response);
								}
							}
						}

						stateGET = false;
						statePOST = false;

					} else if (writeMail) {

						// Prepare for sending mail
						reFwMail = false;

						std::vector<std::string> paramsPOST = StrSplit(command, '&');
						std::string mailTo = url_decode(StrSplit(paramsPOST[0], '=')[1]);
						std::string mailSubject = url_decode(StrSplit(paramsPOST[1], '=')[1]);
						std::string mailMainContent = url_decode(StrSplit(paramsPOST[2], '=')[1]);
						mailMainContent.erase(std::remove(mailMainContent.begin(),
								mailMainContent.end(), '\r'), mailMainContent.end());
						std::vector<std::string> tmp = StrSplit(mailTo, '@');
						std::string mailToName = tmp[0];
						std::string mailToHost = tmp[1];

						if (verbose) std::cout << "[mailToName]" << mailToName << std::endl;
						if (verbose) std::cout << "[mailToHost]" << mailToHost << std::endl;

						std::string mailNumR = RemoveCR(GetServerResponse(
								keyValueAddr,"/GET " + mailTo + " mailNum\r\n", thread_socket));
						if (mailTo == "") {

							// If receiver address is not specified, return error page
							// save the mail for recovery
							if (verbose) std::cout << "[--- Empty MailTo]" << std::endl;
							mailSaved = true;
							writeMail = false;
							operateMail = false;
							savedMailTo = "";
							savedMailSubject = mailSubject;
							savedMailMainContent = mailMainContent;

							std::string content = GetComposeAlertPage("Please input mail receiver.", port);

							std::string response = reqProto + OK_200;
							response += TEXT_HTML + CONTENT_LEN;
							response += std::to_string(content.size());
							response += CONTENT_BEGIN + content;

							SendMessage(thread_socket, response);

						} else if (mailNumR.substr(0, 4) == "-ERR" &&
								mailToHost == "penncloud.com") {

							// If receiver account is a penncloud accout and does not exist, return error page
							// save the mail for recovery

							if (verbose) std::cout << "[--- Invalid MailTo]" << std::endl;
							mailSaved = true;
							writeMail = false;
							operateMail = false;
							savedMailTo = "";
							savedMailSubject = mailSubject;
							savedMailMainContent = mailMainContent;

							std::string content = GetComposeAlertPage("Receiver account does not exist.", port);

							std::string response = reqProto + OK_200;
							response += TEXT_HTML + CONTENT_LEN;
							response += std::to_string(content.size());
							response += CONTENT_BEGIN + content;

							SendMessage(thread_socket, response);

						} else {

							// Begin to send email
							if (verbose) std::cout << "[--- Send Email]" << std::endl;
							// AddNewMail will fail if the mail is attempted to send to external server
							// and cannot find the ip via res_query()
							bool succeed = AddNewMail(keyValueAddr, masterAddr, currentUser, thread_socket, command);

							std::string content;
							if (!succeed) {

								// If send fails, return error page
								// Save mail for recovery
								if (verbose) std::cout << "[--- Invalid Domain]" << std::endl;
								mailSaved = true;
								writeMail = false;
								operateMail = false;
								savedMailTo = "";
								savedMailSubject = mailSubject;
								savedMailMainContent = mailMainContent;
								content = GetComposeAlertPage("Cannot connect receiver domain.", port);

							} else {
								content = GetIndexPageContent(keyValueAddr, currentUser, thread_socket, port);
							}

							// Return to index page
							std::string response = reqProto + OK_200;
							response += TEXT_HTML + CONTENT_LEN;
							response += std::to_string(content.size());
							response += CONTENT_BEGIN + content;

							SendMessage(thread_socket, response);
							writeMail = false;
						}

					} else if (operateMail) {

						// If reply/forward/delete
						// First get current email content
						std::vector<std::string> tmpVec_str = StrSplit(reqPage, '&');
						std::string mailID = tmpVec_str[1];
						if (verbose) std::cout << "[mailID]" << mailID << std::endl;
						std::string reqKV = "/GET " + currentUser +
								" " + mailID + "\r\n";
						std::string mailStr = decode_base64(GetServerResponse(keyValueAddr, reqKV, thread_socket));
						struct Mail mail = ParseMail(mailStr, currentUserMailAddr);

						std::string operation = command;
						operation = operation.substr(0, operation.size() - 1);

						if (operation == "reply") {
							reFwMail = true;

							// Add some indicators for reply to title and body
							std::string replyTo = mail.mailFrom;
							std::string replySubject = "Re: " + mail.mailSubject;
							std::string replyMainContent =
									" \n\n============= Reply =============\n"
									"From: " + mail.mailFrom + "\n" +
									"To: " + mail.mailTo + "\n\n" +
									mail.mailMainContent;

							std::string content = GetSidebar(keyValueAddr,
									currentUser, thread_socket) +
									COMPOSE_BEFORE_FROM +
									currentUserMailAddr +
									GetComposeAfterForm(
											replyTo, replySubject, replyMainContent);

							std::string response = reqProto + OK_200;
							response += TEXT_HTML + CONTENT_LEN;
							response += std::to_string(content.size());
							response += CONTENT_BEGIN + content;

							SendMessage(thread_socket, response);

						} else if (operation == "forward") {
							reFwMail = true;

							// Add some indicators for forward to title and body
							std::string replySubject = "Fw: " + mail.mailSubject;
							std::string replyMainContent =
									" \n\n============= Forward =============\n"
									"From: " + mail.mailFrom + "\n" +
									"To: " + mail.mailTo + "\n\n" +
									mail.mailMainContent;

							std::string content = GetSidebar(keyValueAddr,
									currentUser, thread_socket) +
									COMPOSE_BEFORE_FROM +
									currentUserMailAddr +
									GetComposeAfterForm(
											"", replySubject, replyMainContent);

							std::string response = reqProto + OK_200;
							response += TEXT_HTML + CONTENT_LEN;
							response += std::to_string(content.size());
							response += CONTENT_BEGIN + content;

							SendMessage(thread_socket, response);

						} else if (operation == "delete") {

							// Move mail from current folder to trash
							// Or delete mail in trash
							DeleteMail(keyValueAddr, masterAddr,
									currentUser, thread_socket,
									mailID, mail);

							// Return to index page
							std::string content = GetIndexPageContent(keyValueAddr, currentUser, thread_socket, port);
							std::string response = reqProto + OK_200;
							response += TEXT_HTML + CONTENT_LEN;
							response += std::to_string(content.size());
							response += CONTENT_BEGIN + content;

							SendMessage(thread_socket, response);

						}

						if(verbose) std::cout << "[reFwMail]" << reFwMail << std::endl;
						operateMail = false;

					} else if (StringMatch(keyword, POST)) {

						// Set current state to POST, prepare to parse following request
						if(verbose) std::cout << "Client POST" << std::endl;

						if (statePOST) {
							SendMessage(thread_socket, "-ERR\r\n");

						} else {

							statePOST = true;
							std::vector<std::string> paramsReq = StrSplit(command, ' ');

							reqType = "";
							reqPage = "";
							reqProto = "";

							if (paramsReq.size() >= 1) reqType = paramsReq[0];
							if (paramsReq.size() >= 2) reqPage = paramsReq[1];
							if (paramsReq.size() >= 3) reqProto = paramsReq[2];
						}

					} else {

						if (!stateGET && !statePOST) {
							// If we do not recognize the words.
							SendMessage(thread_socket, "500 Unrecognized command\r\n");
						} else {
							if(verbose) std::cout << command << std::endl;
						}
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


