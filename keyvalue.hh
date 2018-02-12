#ifndef _KEYVALUE_HH
#define _KEYVALUE_HH

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>
#include "cpp-base64/base64.cpp"
using namespace std;

extern bool debug;
extern volatile bool running;
const int BUFFER_SIZE = 4096;

enum Config { other, master, storage, mail, user };
class Item {
public:
	bool is_file; // file or folder
	string name;

	Item(bool is_file, string name) : is_file(is_file), name(name) {}
};

sockaddr_in string_to_sockaddr(string address);
string sockaddr_to_string(sockaddr_in sockaddr);

string join(vector<string>& strs, string delimiter);
vector<string> split(string str, string delimiter);
map<string, string> parse_url_params(string param_str);
string parse_payload(string request);
int get_payload_length(string content);
map<string, string> parse_post_parameters(string payload);
string parse_cookie(string request);
string serialize_items(vector<Item>& items);
vector<Item> deserialize_items(string item_str);
int find_item(vector<Item>& items, string name);
void parse_config(char* file_name, vector<sockaddr_in>& masters, vector<sockaddr_in>& storages,
				  vector<sockaddr_in>& mails, vector<sockaddr_in>& users);

string do_read(int fd);
string http_read(int fd);
bool do_write(int fd, const char* buf, int len);

string keyvalue_operation(string keyvalue_request, string username, sockaddr_in master);
string table_put(string row, string col, string value, string username, sockaddr_in master);
string table_get(string row, string col, string username, sockaddr_in master);
string table_cput(string row, string col, string value1, string value2, string username, sockaddr_in master);
string table_delete(string row, string col, string username, sockaddr_in master);

string encode_base64(string s);
string decode_base64(string s);

// convert string (ip + port) to sockaddr_in
sockaddr_in string_to_sockaddr(string address) {
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

// convert sockaddr_in to string (ip + port)
string sockaddr_to_string(sockaddr_in sockaddr) {
	return string(inet_ntoa(sockaddr.sin_addr)) + ":" + to_string(ntohs(sockaddr.sin_port));
}

// join the vector of strings using the given delimiter
string join(vector<string>& strs, string delimiter) {
	string res = "";
	for (int i = 0; i < strs.size(); i++) {
		if (i != strs.size() - 1) {
			res += (strs[i] + delimiter);
		} else {
			res += strs[i];
		}
	}
	return res;
}

// split the string using the given delimiter
vector<string> split(string str, string delimiter) {
	vector<string> slices;
	int prev = 0, curr = str.find(delimiter);
	while (curr != -1) {
		slices.push_back(str.substr(prev, curr - prev));
		prev = curr + delimiter.length();
		curr = str.find(delimiter, prev);
	}
	slices.push_back(str.substr(prev));
	return slices;
}

// convert url parameters into a map
// e.g. name=aaa&id=111 => ["name"->"aaa", "id"->"111"]
map<string, string> parse_url_params(string param_str) {
	vector<string> params = split(param_str, "&");
	map<string, string> param_map;
	for (auto par : params) {
		int idx = par.find("=");
		if (idx == -1) continue;
		string key = par.substr(0, idx);
		string value = par.substr(idx + 1);
		value = regex_replace(value, regex("%2F"), "/");
		param_map[key] = value;
	}
	return param_map;
}

// get payload of POST request
string parse_payload(string request) {
	string payload = "";
	static const string boundary_label = "Content-Type: multipart/form-data; boundary=";
	if (debug) printf("Parsing POST request body...\n");
	int idx = request.find(boundary_label);
	int end = request.find("\r\n", idx);
	string boundary;
	if (idx != -1 && end != -1) {
		boundary = request.substr(idx + boundary_label.length(), end - idx - boundary_label.length());
		if (debug) printf("Boundary is %s\n", boundary.c_str());
		int bound1 = request.find(boundary, end); // start of the payload, excluding the one in header
		int bound2 = request.find(boundary, bound1 + boundary.length()); // end of the payload
		if (bound1 != -1 && bound2 != -1) {
			bound1 += boundary.length() + 2; // remove boundary + \r\n
			payload = request.substr(bound1, bound2 - bound1 - 4); // remove \r\n
			if (debug) printf("Payload (length %lu) is:\n%s\n", payload.length(), payload.c_str());
		}
	}
	return payload;
}

// get the length of payload
// e.g. "Content-Length: 233"
int get_payload_length(string content) {
	static const string len_label = "Content-Length: ";
	int len = INT_MAX;
	int idx = content.find(len_label);
	int end = content.find("\r\n", idx);
	if (idx != -1 && end != -1) { // find content length
		string len_str = content.substr(idx + len_label.length(), end - idx - len_label.length());
		len = stoi(len_str);
		if (debug) printf("HTTP request with content length %d.\n", len);
	}
	return len;
}

// convert POST request parameters into a map
// e.g. Content-Disposition: form-data; name="upload_file"; filename="config.txt"
map<string, string> parse_post_parameters(string payload) {
	map<string, string> param_map;
	static const string params_label = "Content-Disposition: ";
	int start = payload.find(params_label);
	int end = payload.find("\n", start + params_label.length());
	if (start != -1 && end != -1) {
		start += params_label.length();
		string param_str = payload.substr(start, end - start - 1); // remove \n
		vector<string> params = split(param_str, "; ");
		for (auto par : params) {
			int idx = par.find("=");
			if (idx == -1) continue;
			string key = par.substr(0, idx);
			string value = par.substr(idx + 1);
			value = value.substr(1, value.length() - 2); // remove quotes
			param_map[key] = value;
		}
	}
	return param_map;
}

// get username from cookie
// e.g. Cookie: name:test%40gmail.com;password:test;SameSite:Lax
string parse_cookie(string request) {
	map<string, string> param_map;
	static const string cookie = "Cookie: ";
	int start = request.find(cookie);
	int end = request.find("\n", start + cookie.length());
	if (start != -1 && end != -1) {
		start += cookie.length();
		string param_str = request.substr(start, end - start + 1); // remove \n
		vector<string> params = split(param_str, "; ");
		for (auto par : params) {
			int idx = par.find(":");
			if (idx == -1) continue;
			string key = par.substr(0, idx);
			string value = par.substr(idx + 1);
			value = regex_replace(value, regex("%2F"), "/");
			param_map[key] = value;
		}
	}
	if (param_map.find("name") != param_map.end()) {
		return param_map["name"];
	} else {
		if (debug) printf("[Cookie] Can't find username in Cookie!\n%s\n", request.c_str());
		return "";
	}
}

// convert a vector of items into item string
// e.g. "[F@test||D@test_dir||F@newfile]"
string serialize_items(vector<Item>& items) {
	vector<string> temp;
	for (auto item : items) {
		string item_type = item.is_file ? "F" : "D";
		temp.push_back(item_type + "@" + item.name);
	}
	return "[" + join(temp, "||") + "]";
}

// convert item string into a vector of items
// e.g. "[F@test||D@test_dir||F@newfile]"
vector<Item> deserialize_items(string item_str) {
	vector<string> temp = split(item_str.substr(1, item_str.length() - 2), "||");
	vector<Item> items;
	for (auto item : temp) {
		int at_idx = item.find("@");
		if (at_idx == -1) continue;
		bool is_file = item.substr(0, at_idx) == "F";
		string item_name = item.substr(at_idx + 1);
//		string item_path = path + item_name;
		items.push_back(Item(is_file, item_name));
	}
	return items;
}

// find a file/folder using given name
// return its index, -1 for not found
int find_item(vector<Item>& items, string name) {
	int j = -1;
	for (int i = 0; i < items.size(); i++) {
		if (items[i].name == name) {
			j = i;
			break;
		}
	}
	return j;
}

// read server configuration file
void parse_config(char* file_name, vector<sockaddr_in>& masters, vector<sockaddr_in>& storages,
				  vector<sockaddr_in>& mails, vector<sockaddr_in>& users) {
	Config curr_config = other;
	FILE* pFile = fopen(file_name, "r");
	char buff[256];
	sockaddr_in serv_addr;
	while (fgets(buff, 256, pFile) != NULL) {
		string str(buff);
		int idx = str.find('\n');
		if (idx != -1) {
			str = str.substr(0, idx);
		}

		if (str.find("MASTER") == 0) {
			curr_config = master;
		} else if (str.find("STORAGE") == 0) {
			curr_config = storage;
		} else if (str.find("MAIL") == 0) {
			curr_config = mail;
		} else if (str.find("USER") == 0) {
			curr_config = user;
		} else {
			if (curr_config == master) {
				masters.push_back(string_to_sockaddr(str));
			} else if (curr_config == storage) {
				storages.push_back(string_to_sockaddr(str));
			} else if (curr_config == mail) {
				mails.push_back(string_to_sockaddr(str));
			} else if (curr_config == user) {
				users.push_back(string_to_sockaddr(str));
			}
		}
	}
}

// read *one line* from the file descriptor
string do_read(int fd) {
	char buffer[BUFFER_SIZE];
	int ptr = 0, cnt = 0;
	string content;
	struct pollfd pfds[1];
	pfds[0].fd = fd;
	pfds[0].events = POLLIN;

	while (running) {
		if (ptr < cnt) {
			content += buffer[ptr++];
			if (content.length() >= 2 && content.substr(content.length() - 2) == "\r\n") {
				break;
			}
		} else {
			int res = poll(pfds, 1, 1000);
			if (res == 0) {
				continue;
			} else if (res == -1) {
				// perror("Poll");
			} else if (pfds[0].revents && POLLIN) {
				cnt = read(fd, buffer, BUFFER_SIZE);
				ptr = 0;
				if (cnt <= 0) break;
			}
		}
	}
	return content;
}

// read entire http request and its payload
string http_read(int fd) {
	char buffer[BUFFER_SIZE];
	int ptr = 0, cnt = 0;
	int len = INT_MAX;
	string content;
	struct pollfd pfds[1];
	pfds[0].fd = fd;
	pfds[0].events = POLLIN;

	while (running) {
		if (ptr < cnt) {
			content += buffer[ptr++];
			if (content.find("POST") == 0) { // POST request, read the entire header and payload
				if (len == INT_MAX) {
					len = get_payload_length(content);
				} else {
					int split = content.find("\r\n\r\n"); // split between header and payload
					if (split != -1 && content.length() - split - 4 >= len) { // payload length >= len
						break;
					}
				}
			} else if (content.find("GET") == 0) { // GET request, read first line, ignore header
				if (content.length() >= 2 && content.substr(content.length() - 2) == "\r\n") { // TODO
					if (content.find("Cookie: ") != -1 /*|| content.find("/?username=") != -1*/) {
						break;
					}
				}
			}
		} else {
			int res = poll(pfds, 1, 1000);
			if (res == 0) {
				continue;
			} else if (res == -1) {
				// perror("Poll");
			} else if (pfds[0].revents && POLLIN) {
				cnt = read(fd, buffer, BUFFER_SIZE);
				ptr = 0;
				if (cnt <= 0) break;
			}
		}
	}
	return content;
}

// write the specified number of characters from buffer to the file descriptor
bool do_write(int fd, const char* buf, int len) {
	int sent = 0;
	while (sent < len){
		int n = write(fd, &buf[sent], len - sent);
		if (n < 0){
			return false;
		}
		sent += n;
	}
	return true;
}

// send the request to key-value storage
// first connect to master to figure out which replica to use
// then connect to the assigned replica for executing the request
string keyvalue_operation(string keyvalue_request, string username, sockaddr_in master) {
	string master_request = "/STOR " + username + "\r\n";

	// connect to master
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Socket");
		return "-ERR Can't create socket";
	}
	connect(sock, (struct sockaddr*)&master, sizeof(master));
	do_write(sock, master_request.c_str(), master_request.length());
	if (debug) printf("Storage -> Master: %s", master_request.c_str());
	string master_response = do_read(sock);
	if (debug) printf("Master -> Storage: %s", master_response.c_str());
	close(sock);

	// connect to key-value storage
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Socket");
		return "-ERR Can't create socket";
	}
	if (master_response == "") {
		master_response = "-ERR Master is down";
	}
	if (master_response.find("+OKIP") == -1) {
		if (debug) printf("%s", master_response.c_str());
		return master_response;
	}
	int idx = master_response.find(" ");
	sockaddr_in serv = string_to_sockaddr(master_response.substr(idx + 1));
	connect(sock, (struct sockaddr*)&serv, sizeof(serv));
	do_write(sock, keyvalue_request.c_str(), keyvalue_request.length());
	if (debug) printf("Storage -> KeyValue: %s", keyvalue_request.c_str());
	string keyvalue_response = do_read(sock);
	if (debug) printf("KeyValue -> Storage: %s", keyvalue_response.c_str());
	return keyvalue_response.substr(0, keyvalue_response.length() - 2);
}

string table_put(string row, string col, string value, string username, sockaddr_in master) {
	string request = "/PUT " + row + " " + col + " " + value + "\r\n";
	return keyvalue_operation(request, username, master);
}

string table_get(string row, string col, string username, sockaddr_in master) {
	string request = "/GET " + row + " " + col + "\r\n";
	return keyvalue_operation(request, username, master);
}

string table_cput(string row, string col, string value1, string value2, string username, sockaddr_in master) {
	string request = "/CPUT " + row + " " + col + " " + value1 + " " + value2 + "\r\n";
	return keyvalue_operation(request, username, master);
}

string table_delete(string row, string col, string username, sockaddr_in master) {
	string request = "/DELETE " + row + " " + col + "\r\n";
	return keyvalue_operation(request, username, master);
}

string encode_base64(string s) {
	return base64_encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.length());
}

string decode_base64(string s) {
	return base64_decode(s);
}

#endif
