#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>
#include <sys/poll.h>
#include <limits.h>
#include <string>
#include <algorithm>
#include <vector>
#include "admin.hh"
#include "keyvalue.hh"
using namespace std;

bool debug = false;
volatile bool running = true;

enum t_status {available, in_use, finished};
vector<thread> threads;
vector<t_status> thread_status;
vector<sockaddr_in> master_servers;
vector<sockaddr_in> storage_servers;
vector<sockaddr_in> mail_servers;
vector<sockaddr_in> user_servers;

void int_handler(int sig);
void worker(int comm_fd, int thread_id);

string render_error_page(string message);
string render_admin_page();
string render_status_page(string status);
string render_view_page(string status);
string render_data_page(string data);
string get_server_status(sockaddr_in master);
string get_table_data(sockaddr_in serv);
void parse_node_status(vector<Node>& nodes, string line, string type);
void set_primary_node(vector<Node>& nodes, string line, string type);

void int_handler(int sig) {
	if (debug) fprintf(stderr, "Shutting down the server...\n");
	running = false;
}

string render_error_page(string message) {
	string response = error_404;
	response += header;
	response += "<h2>" + message + "</h2>\n";
	response += go_back;
	response += footer;
	return response;
}

string render_admin_page() {
	string response = ok_200;
	response += header;
	response += navbar_header + "admin" + navbar_footer;

	response += "<div class=\"container\">\n";
    response += "<header class=\"jumbotron\">\n";
    response += "<div class=\"container\">\n";
    response += "<h2><i class=\"fa fa-server fa-fw\" aria-hidden=\"true\"></i>My Servers</h2>\n";
	response += "<a href=\"/admin/status\" class=\"btn btn-success\">Node Status</a>\n";
	response += "<a href=\"/admin/view\" class=\"btn btn-info\">View Key-Value Storage</a>\n";
	response += "</div>\n";
    response += "</header>\n\n";

	response += "</div>\n";
	response += footer;
	return response;
}

string render_status_page(string status) {
	string response = ok_200;
	response += header;
	response += navbar_header + "admin" + navbar_footer;

	string curr_config = "Other";
	vector<Node> nodes;
	vector<string> lines = split(status, "\n");
	for (auto line : lines) {
		if (line.find(" node count: ") != -1) {
			curr_config = line.substr(0, line.find(" "));
		}

		if (curr_config == "Other") {
			if (line.find("Local time") == 0) {
				response += "<div>" + line + "</div>\n";
			}
		} else if (curr_config == "Master") {
			int idx = line.find(": ");
			if (idx != -1) {
				nodes.push_back(Node(0, "Master", line.substr(idx + 2), "Checked", true));
			}
		} else { // Storage, Mail, User
			if (isdigit(line[0])) {
				parse_node_status(nodes, line, curr_config);
			} else if (line.find(curr_config + " primary replica index: ") == 0) {
				set_primary_node(nodes, line, curr_config);
			}
		}
	}

	response += table_header;
	for (auto node : nodes) {
		if (node.status == "Checked") {
			response += "<tr class=\"success\">\n";
		} else {
			response += "<tr class=\"danger\">\n";
		}
		response += "<td>" + to_string(node.id) + "</td>\n";
		response += "<td>" + node.type + "</td>\n";
		response += "<td>" + node.address + "</td>\n";
		response += "<td>" + node.status + "</td>\n";
		string is_primary = node.primary ? "<strong>Yes</strong>" : "No";
		response += "<td>" + is_primary + "</td>\n";
		response += "<td></td>\n";
		response += "</tr>\n";
	}
	response += table_footer;
	response += footer;
	return response;
}

string render_view_page(string status) {
	string response = ok_200;
	response += header;
	response += navbar_header + "admin" + navbar_footer;

	string curr_config = "Other";
	vector<Node> nodes;
	vector<string> lines = split(status, "\n");
	for (auto line : lines) {
		if (line.find(" node count: ") != -1) {
			curr_config = line.substr(0, line.find(" "));
		}

		if (curr_config == "Other") {
			// if (line.find("Local time") == 0) {
			// 	response += "<div>" + line + "</div>\n";
			// }
		} else if (curr_config == "Master") {
			int idx = line.find(": ");
			if (idx != -1) {
				nodes.push_back(Node(0, "Master", line.substr(idx + 2), "Checked", true));
			}
		} else { // Storage, Mail, User
			if (isdigit(line[0])) {
				parse_node_status(nodes, line, curr_config);
			} else if (line.find(curr_config + " primary replica index: ") == 0) {
				set_primary_node(nodes, line, curr_config);
			}
		}
	}

	response += table_header;
	for (auto node : nodes) {
		if (node.status == "Checked") {
			response += "<tr class=\"success\">\n";
		} else {
			response += "<tr class=\"danger\">\n";
		}
		response += "<td>" + to_string(node.id) + "</td>\n";
		response += "<td>" + node.type + "</td>\n";
		response += "<td>" + node.address + "</td>\n";
		response += "<td>" + node.status + "</td>\n";
		string is_primary = node.primary ? "<strong>Yes</strong>" : "No";
		response += "<td>" + is_primary + "</td>\n";
		if (node.status == "Checked") {
			response += "<td><a href=\"/admin/view/" + encode_base64(node.address) + "\">View Data</a></td>\n";
		} else {
			response += "<td></td>\n";
		}
		response += "</tr>\n";
	}
	response += table_footer;
	response += footer;
	return response;
}

string render_data_page(string data, string address) {
	string response = ok_200;
	response += header;
	response += navbar_header + "admin" + navbar_footer;
	response += "<h2>" + address + "</h2>";

	vector<Value> values;
	vector<string> lines = split(data, "#");
	for (auto line : lines) {
		vector<string> params = split(line, " ");
		if (params.size() != 3) {
			continue;
		}
		values.push_back(Value(params[0], params[1], params[2]));
	}

	response += data_header;
	for (auto x : values) {
		response += "<tr>\n";
		response += "<td>" + x.row + "</td>\n";
		response += "<td>" + x.col + "</td>\n";
		if (x.val.length() > 50) {
			response += "<td>[" + to_string(x.val.length()) + " bytes of data]</td>\n";
		} else {
			response += "<td>" + x.val + "</td>\n";
		}
		response += "</tr>\n";
	}
	response += data_footer;
	response += footer;
	return response;
}

// contact master node to get status of all nodes
string get_server_status(sockaddr_in master) {
	string master_request = "/ADMIN\r\n";

	// connect to master
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Socket");
		return "-ERR Can't create socket";
	}
	connect(sock, (struct sockaddr*)&master, sizeof(master));
	do_write(sock, master_request.c_str(), master_request.length());
	if (debug) printf("Admin -> Master: %s", master_request.c_str());
	string master_response = do_read(sock);
	if (debug) printf("Master -> Admin: %s", master_response.c_str());
	close(sock);

	return master_response;
}

// contact a key-value storage node get raw data for viewing
string get_table_data(sockaddr_in serv) {
	// connect to keyvalue storage
	string keyvalue_request = "/ADM 0 100\r\n";
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Socket");
		return "-ERR Can't create socket";
	}
	connect(sock, (struct sockaddr*)&serv, sizeof(serv));
	do_write(sock, keyvalue_request.c_str(), keyvalue_request.length());
	if (debug) printf("Admin -> KeyValue: %s", keyvalue_request.c_str());
	string keyvalue_response = do_read(sock);
	if (debug) printf("KeyValue -> Admin: %s", keyvalue_response.c_str());
	return keyvalue_response;
}

// parse node status from string response and put them in the nodes vector
void parse_node_status(vector<Node>& nodes, string line, string type) {
	vector<string> params = split(line, " ");
	int id = stoi(params[0].substr(0, params[0].length() - 1));
	string address = params[1];
	string status = params[3];
	for (int i = 4; i < params.size(); i++) {
		status += " " + params[i];
	}
	nodes.push_back(Node(id, type, address, status, false));
}

// mark primary nodes in the nodes vector
void set_primary_node(vector<Node>& nodes, string line, string type) {
	int idx = line.find(": ");
	int primary_id = stoi(line.substr(idx + 2));
	for (auto iter = nodes.begin(); iter != nodes.end(); iter++) {
		if (iter->id == primary_id && iter->type == type) {
			iter->primary = true;
		}
	}
}

void worker(int comm_fd, int thread_id) {
	char buffer[BUFFER_SIZE];
	int ptr = 0, cnt = 0;
	
	// read the request message
	string request = do_read(comm_fd);
	string response;
	if (debug) fprintf(stderr, "[%d] C: %s", comm_fd, request.c_str());
	if (request.length() == 0) {
		if (debug) printf("Empty request ignored.\n");
		response = error_404;
		write(comm_fd, response.c_str(), response.length());
		close(comm_fd);
		thread_status[thread_id] = finished;
		return;
	}
	
	// handle the request
	vector<string> params = split(request, " ");
	string req_type = params[0];
	string req_page = params[1];
	map<string, string> req_params;
	int param_start = req_page.find("?");
	if (param_start != -1) {
		req_params = parse_url_params(req_page.substr(param_start + 1));
		req_page = req_page.substr(0, param_start);
	}
	string req_protocol = params[2];
		
	if (req_page == "/") {
		req_page = "/admin";
	}
	
	if (req_page == "/admin") {
		response = render_admin_page();
	} else if (req_page == "/admin/status") {
		string status = get_server_status(master_servers[0]);
		response = render_status_page(status);
	} else if (req_page == "/admin/view") {
		string status = get_server_status(master_servers[0]);
		response = render_view_page(status);
	} else if (req_page.find("/admin/view/") == 0) {
		string address = decode_base64(req_page.substr(req_page.rfind("/") + 1));
		string data = get_table_data(string_to_sockaddr(address));
		response = render_data_page(data, address);
	} else {
		response = render_error_page("404 Not Found");
	}
	
	// send the response message
	write(comm_fd, response.c_str(), response.length());
	if (debug) fprintf(stderr, "[%d] S: %s", comm_fd, response.c_str());

	if (debug) fprintf(stderr, "[%d] Finishing current thread\n", comm_fd);
	close(comm_fd);
	thread_status[thread_id] = finished;
}

int main(int argc, char *argv[]) {
	int opt;
	int port = 8080;
	while ((opt = getopt(argc, argv, "p:av")) != -1) {
		if (opt == 'p') {
			port = atoi(optarg);
		} else if (opt == 'v') {
			debug = true;
		} else {
			fprintf(stderr, "[Admin] Unrecognized option -%c.\n", opt);
			exit(1);
		}
	}
	if (optind >= argc) {
		fprintf(stderr, "[Admin] Please specify a configuration file.\n");
		exit(1);
	}
	char* config_file = argv[optind];
	
	// read the configuration file
	parse_config(config_file, master_servers, storage_servers, mail_servers, user_servers);
	
	running = true;
	signal(SIGINT, int_handler);
	signal(SIGPIPE, SIG_IGN); // temporary fix

	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("[Admin] Socket");
		exit(1);
	}
	int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        perror("[Admin] Setsockopt");
        exit(1);
    }
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(port);
	if (bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror("[Admin] Bind");
		exit(1);
	}
	if (listen(listen_fd, 10) < 0) {
		perror("[Admin] Listen");
		exit(1);
	}
	
	struct pollfd pfds[1];
	pfds[0].fd = listen_fd;
	pfds[0].events = POLLIN;
	while (running) {
		int res = poll(pfds, 1, 1000);
		if (res == 0) {
			continue;
		} else if (res == -1) {
			// perror("Poll");
		} else if (pfds[0].revents && POLLIN) {
			struct sockaddr_in clientaddr;
			socklen_t clientaddrlen = sizeof(clientaddr);
			int fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
			if (debug) fprintf(stderr, "[Admin %d] New connection from %s\n", fd, inet_ntoa(clientaddr.sin_addr));
			
			// clean all finished threads
            for (int i = 0; i < threads.size(); i++) {
                if (thread_status[i] == finished) {
                    threads[i].join();
                    thread_status[i] = available;
                }
            }

            // create a new thread
            bool found_available = false;
            for (int i = 0; i < threads.size(); i++) {
                if (thread_status[i] == available) {
                    // threads[i] = new_thread;
                    threads[i] = thread(worker, fd, i);
                    thread_status[i] = in_use;
                    found_available = true;
                    break;
                }
            }
            if (!found_available) {
                threads.push_back(thread(worker, fd, threads.size()));
                thread_status.push_back(in_use);
            }
		}
	}
	
	if (debug) fprintf(stderr, "[Admin] Joining %lu threads...\n", threads.size());
    for (int i = 0; i < threads.size(); i++) {
        threads[i].join();
    }
	if (debug) fprintf(stderr, "[Admin] Closing the server connection...\n");
	close(listen_fd);
	return 0;
}
