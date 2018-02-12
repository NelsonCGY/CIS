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
#include "storage.hh"
#include "keyvalue.hh"
using namespace std;

bool debug = false;
volatile bool running = true;
int port = 8000;

enum t_status {available, in_use, finished};
vector<thread> threads;
vector<t_status> thread_status;
vector<sockaddr_in> master_servers;
vector<sockaddr_in> storage_servers;
vector<sockaddr_in> mail_servers;
vector<sockaddr_in> user_servers;
// string username, password; // TODO: temporary (allow one user only)

void int_handler(int sig);
void worker(int comm_fd, int thread_id);

string render_login_page(string login_url);
string render_error_page(string message, string username);
string render_error_page(string message, string username, string cookies);
string render_view_page(string path, vector<Item> items, string username);
string render_upload_page(string path, string content, string username);
string render_download_page(string path, string content, string username);
string render_folder_page(string path, string content, string username);
string render_rename_form(string path, string content, string username);
string render_rename_page(string path, string content, string username);
string render_move_form(string path, string content, string username);
string render_move_page(string path, string content, string username);
string render_delete_page(string path, string content, string username);

void int_handler(int sig) {
    if (debug) fprintf(stderr, "Shutting down the server...\n");
    running = false;
}

string render_login_page(string login_url) {
    string response = ok_200;
    response += header;
    response += navbar_header + "Anonymous" + navbar_footer;
    response += "<h2>Please <a href=\"" + login_url + "\">login</a></h2>\n";
    response += footer;
    return response;
}

string render_view_page(string path, vector<Item> items, string username, string cookies) {
    string response = "HTTP/1.1 200 OK\r\n";
    response += cookies;
    response += "Content-Type: text/html\r\n\r\n";
    response += header;
    response += navbar_header + username + navbar_footer;

    response += "<div class=\"container\">\n";
    response += "<header class=\"jumbotron\">\n";
    response += "<div class=\"container\">\n";
    response += "<h2><i class=\"fa fa-floppy-o fa-fw\" aria-hidden=\"true\"></i>My Drive</h2>\n";

    // upload form
    response += "<form name=\"upload_form\" action=\"/upload" + path + "\" method=\"POST\" enctype=\"multipart/form-data\">\n";
    response += "<div class=\"input-group\">\n";
    response += "<input type=\"file\" class=\"form-control\" name=\"upload_file\">\n";
    response += "<span class=\"input-group-btn\">\n";
    response += "<button class=\"btn btn-default\" type=\"submit\">Upload File</button>\n";
    response += "</span>\n";
    response += "</div>\n";
    response += "</form>\n";

    // folder form
    response += "<form name=\"folder_form\" action=\"/folder" + path + "\">\n";
    response += "<div class=\"input-group\">\n";
    response += "<input type=\"text\" class=\"form-control\" name=\"new_folder\">\n";
    response += "<span class=\"input-group-btn\">\n";
    response += "<button class=\"btn btn-success\" type=\"submit\">New Folder</button>\n";
    response += "</span>\n";
    response += "</div>\n";
    response += "</form>\n";

    response += "</div>\n";
    response += "</header>\n\n";

    // file table
    response += "<h2>" + path + "</h2>\n";
    response += table_header;
    for (auto item : items) {
        string name = item.name;
        if (!item.is_file) name += "/";
        response += "<tr>\n";
        if (item.is_file) {
            response += "<td><i class=\"fa fa-file fa-fw\" aria-hidden=\"true\"></i><a href=\"/download" + path + name + "\">" + item.name + "</a></td>\n";
        } else {
            response += "<td><i class=\"fa fa-folder fa-fw\" aria-hidden=\"true\"></i><a href=\"/view" + path + name + "\">" + item.name + "</a></td>\n";
        }
        response += "<td>\n";
        if (item.is_file) { // no download button for folders so far
            response += "<a href=\"/download" + path + name + "\" class=\"btn btn-primary\"><i class=\"fa fa-download fa-fw\" aria-hidden=\"true\"></i>Download</a>\n";
            response += "<a href=\"/rename_form" + path + name + "\" class=\"btn btn-warning\"><i class=\"fa fa-pencil-square fa-fw\" aria-hidden=\"true\"></i>Rename</a>\n";
            response += "<a href=\"/move_form" + path + name +"\" class=\"btn btn-info\"><i class=\"fa fa-arrow-right fa-fw\" aria-hidden=\"true\"></i>Move</a>\n";
        }
        response += "<a href=\"/delete" + path + name + "\" class=\"btn btn-danger\"><i class=\"fa fa-trash fa-fw\" aria-hidden=\"true\"></i>Delete</a>\n";
        response += "</td>\n";
        response += "</tr>\n";
    }
    response += table_footer;

    response += "</div>\n";
    response += footer;
    return response;
}

string render_upload_page(string path, string content, string username) {
    string response = ok_200;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>Uploaded to " + path + "</h2>\n";
    response += go_back;
    response += footer;
    return response;
}

string render_download_page(string path, string content, string username) {
    string response = ok_200_file;
    response += content;
    return response;
}

string render_folder_page(string path, string content, string username) {
    string response = ok_200;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>Created a new folder " + path + "</h2>\n";
    response += go_back;
    response += footer;
    return response;
}

string render_rename_form(string path, string content, string username) {
    string response = ok_200;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>Rename " + path + "</h2>\n";
    response += "<form name=\"rename_form\" action=\"/rename" + path + "\">\n";
    response += "<p>New name for the file</p>";
    response += "<input type=\"text\" name=\"new_name\">\n";
    response += "<button type=\"submit\">Rename</button>\n";
    response += "</form>\n";
    return response;
}

string render_rename_page(string path, string content, string username) {
    string response = ok_200;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>Renamed it from " + path + " to " + content + "</h2>\n";
    response += go_back;
    response += footer;
    return response;
}

string render_move_form(string path, string content, string username) {
    string response = ok_200;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>Move " + path + "</h2>\n";
    response += "<form name=\"move_form\" action=\"/move" + path + "\">\n";
    response += "<p>New path for the file</p>";
    response += "<input type=\"text\" name=\"new_path\">\n";
    response += "<button type=\"submit\">Move</button>\n";
    response += "</form>\n";
    return response;
}

string render_move_page(string path, string content, string username) {
    string response = ok_200;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>Moved it from " + path + " to " + content + "</h2>\n";
    response += go_back;
    response += footer;
    return response;
}

string render_delete_page(string path, string content, string username) {
    string response = ok_200;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>Deleted from " + path + "</h2>\n";
    response += go_back;
    response += footer;
    return response;
}

string render_error_page(string message, string username) {
    string response = error_404;
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>" + message + "</h2>\n";
    response += go_back;
    response += footer;
    return response;
}

string render_error_page(string message, string username, string cookies) {
    string response = "HTTP/1.1 404 Not Found\r\n";
    response += cookies;
    response += "Content-Type: text/html\r\n\r\n";
    response += header;
    response += navbar_header + username + navbar_footer;
    response += "<h2>" + message + "</h2>\n";
    response += go_back;
    response += footer;
    return response;
}

void worker(int comm_fd, int thread_id) {
    char buffer[BUFFER_SIZE];
    int ptr = 0, cnt = 0;

    // read the request message
    string request = http_read(comm_fd);
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
    int first_newline = request.find("\r\n");
    vector<string> params = split(request.substr(0, first_newline), " ");
    string req_type = params[0];
    string req_page = params[1];
    map<string, string> req_params;
    int param_start = req_page.find("?");
    if (param_start != -1) {
        req_params = parse_url_params(req_page.substr(param_start + 1));
        req_page = req_page.substr(0, param_start);
    }
    string req_protocol = params[2];
    string payload = "";
    if (req_type == "POST") {
        payload = parse_payload(request);
        req_params = parse_post_parameters(payload);
        int payload_start = payload.find("\r\n\r\n");
        if (payload_start != -1) {
            payload = payload.substr(payload_start + 4);
            if (debug) printf("Actual parsed payload length: %lu\n", payload.length());
        }
    }

    static const string cookie = "Cookie: name=";
    int start = request.find(cookie);
    int end = request.find(";", start + cookie.length());
    string username = "";
    if (start != -1 && end != -1) {
        username = request.substr(start + cookie.length(), end - start - cookie.length());
        cout << "Username is " << username << endl;
    }
    if (username == "") { // not logged in
        if (req_params.find("username") != req_params.end()) {
            username = req_params["username"];
        } else {
            response = render_login_page("http://localhost:10000/");
            if (debug) printf("[Drive] User not log in.\n");
            write(comm_fd, response.c_str(), response.length());
            close(comm_fd);
            thread_status[thread_id] = finished;
            return;
        }
    } else {
        if (debug) printf("[Drive] Username is %s\n", username.c_str());
    }

    // create a new drive (should be done when creating a new user)
    if (table_get(username + "@" + "/", "items", username, master_servers[0]).find("-ERR") == 0) {
        if (debug) printf("Creating new drive for user %s...\n", username.c_str());
        table_put(username + "@" + "/", "items", "[]", username, master_servers[0]);
    }

    if (req_page == "/") {
        req_page = "/view/";
    }
    int idx = req_page.find("/", 1); // find the second "/"
    if (idx == -1) {
        response = render_error_page("404 Not Found", username);
        write(comm_fd, response.c_str(), response.length());
        close(comm_fd);
        thread_status[thread_id] = finished;
        return;
    }
    string path = req_page.substr(idx);

    if (req_page.find(view_route) == 0) { // view files and folders
        string get_result = table_get(username + "@" + path, "items", username, master_servers[0]);
        string cookies = "";
        if (req_params.find("username") != req_params.end()) {
            cookies += "Set-Cookie: name=" + username + "\r\n"; 
            cookies += "Set-Cookie: password=***\r\n";
        }

        if (debug) printf("[Drive] Set-Cookie for %s.\n", username.c_str());
        if (get_result.find("-ERR") == 0) {
            response = render_error_page("Folder doesn't exist", username, cookies); // view a folder that doesn't exist
        } else {
            string json = get_result;
            if (debug) printf("View JSON (len = %lu): %s\n", json.length(), json.c_str());
            vector<Item> files = deserialize_items(json);
            response = render_view_page(path, files, username, cookies);
        }
    } else if (req_page.find(upload_route) == 0) { // upload a file
        string file_name = req_params["filename"]; // get file name from request
        string get_result = table_get(username + "@" + path, "items", username, master_servers[0]);
        if (get_result.find("-ERR") == 0) {
            response = render_error_page("Folder doesn't exist", username); // upload file to a folder that doesn't exist
        } else if (file_name == "") {
            response = render_error_page("Please select a file", username); // no file is selected
        } else if (file_name.find("/") != -1) {
            response = render_error_page("Filename can't include /", username);
        } else {
            string json = get_result;
            if (debug) printf("Upload JSON: %s\n", json.c_str());
            vector<Item> items = deserialize_items(json);
            int j = find_item(items, file_name);

            if (j != -1) {
                response = render_error_page("File already exists", username);
            } else {
                // add new file to the json, and put it back to key-value storage
                items.push_back(Item(true, file_name));
                table_cput(username + "@" + path, "items", json, serialize_items(items), username, master_servers[0]);
                // add file content
                string file_content = encode_base64(payload);
                table_put(username + "@" + path + file_name, "content", file_content, username, master_servers[0]);
                response = render_upload_page(path + file_name, file_content, username);
            }
        }
    } else if (req_page.find(download_route) == 0) { // download a file
        string get_result = table_get(username + "@" + path, "content", username, master_servers[0]);
        if (get_result.find("-ERR") == 0) {
            response = render_error_page("File doesn't exist", username); // files doesn't exist
        } else {
            string file_content = decode_base64(get_result);
            response = render_download_page(path, file_content, username);
        }
    } else if (req_page.find(folder_route) == 0) { // create a folder
        string get_result = table_get(username + "@" + path, "items", username, master_servers[0]);
        if (get_result.find("-ERR") == 0) {
            response = render_error_page("Parent folder doesn't exist", username); // parent folder doesn't exist
        } else {
            string json = get_result;
            if (debug) printf("Folder JSON: %s\n", json.c_str());
            vector<Item> items = deserialize_items(json);
            int j = find_item(items, req_params["new_folder"]);

            if (j != -1) { // folder or file with the same name already exists
                response = render_error_page("Same file/folder name already exists", username);
            } else {
                // add it to parent folder's item list
                items.push_back(Item(false, req_params["new_folder"]));
                table_cput(username + "@" + path, "items", json, serialize_items(items), username, master_servers[0]);
                // create a new folder
                table_put(username + "@" + path + req_params["new_folder"] + "/", "items", "[]", username, master_servers[0]);
                response = render_folder_page(path + req_params["new_folder"] + "/" , "", username);
            }
        }
    } else if (req_page.find(rename_form_route) == 0) { // rename a file (get new name)
        response = render_rename_form(path, "", username);
    } else if (req_page.find(rename_route) == 0) { // rename a file (actual renaming)
        int idx = path.rfind("/");
        string parent_path = path.substr(0, idx + 1);
        string file_name = path.substr(idx + 1);
        string get_result = table_get(username + "@" + parent_path, "items", username, master_servers[0]);
        if (get_result.find("-ERR") == 0) {
            response = render_error_page("Parent folder doesn't exist", username);
        } else if (req_params["new_name"].find("/") != -1) {
            response = render_error_page("Filename can't include /", username);
        } else {
            string json = get_result;
            if (debug) printf("Rename JSON: %s\n", json.c_str());
            vector<Item> items = deserialize_items(json);
            int j = find_item(items, file_name);

            if (j == -1) {
                response = render_error_page("File doesn't exist", username);
            } else {
                int k = find_item(items, req_params["new_name"]);
                if (k != -1) {
                    response = render_error_page("File with the same name already exists", username);
                } else {
                    // change the name from parent folder's item list
                    items[j].name = req_params["new_name"];
                    table_cput(username + "@" + parent_path, "items", json, serialize_items(items), username, master_servers[0]);
                    // delete the file and upload it again with new name
                    string file_content = table_get(username + "@" + path, "content", username, master_servers[0]);
                    table_delete(username + "@" + path, "content", username, master_servers[0]);
                    table_put(username + "@" + parent_path + req_params["new_name"], "content", file_content, username, master_servers[0]);
                    response = render_rename_page(path, req_params["new_name"], username);
                }
            }
        }
    } else if (req_page.find(move_form_route) == 0) { // move a file (get new path)
        response = render_move_form(path, "", username);
    } else if (req_page.find(move_route) == 0) { // move a file (actual moving)
        // source
        int idx = path.rfind("/");
        string parent_path_src = path.substr(0, idx + 1);
        string file_name_src = path.substr(idx + 1);
        string get_result_src = table_get(username + "@" + parent_path_src, "items", username, master_servers[0]);
        // destination
        idx = req_params["new_path"].rfind("/");
        string parent_path_dest = req_params["new_path"].substr(0, idx + 1);
        string file_name_dest = req_params["new_path"].substr(idx + 1);
        string get_result_dest = table_get(username + "@" + parent_path_dest, "items", username, master_servers[0]);

        if (get_result_src.find("-ERR") == 0 || get_result_dest.find("-ERR") == 0) {
            response = render_error_page("Source/destination parent folder doesn't exist", username);
        } else {
            string json_src = get_result_src;
            string json_dest = get_result_dest;
            if (debug) printf("Move JSON (Source): %s\n", json_src.c_str());
            if (debug) printf("Move JSON (Destination): %s\n", json_dest.c_str());
            vector<Item> items_src = deserialize_items(json_src);
            vector<Item> items_dest = deserialize_items(json_dest);
            int j = find_item(items_src, file_name_src);
            int k = find_item(items_dest, file_name_dest);

            if (j == -1) {
                response = render_error_page("Source file doesn't exist", username);
            } else if (k != -1) {
                response = render_error_page("Destination file already exists", username);
            } else {
                // remove the file from source folder and add it to the destination folder
                items_src.erase(items_src.begin() + j);
                table_cput(username + "@" + parent_path_src, "items", json_src, serialize_items(items_src), username, master_servers[0]);
                items_dest.push_back(Item(true, file_name_dest));
                table_cput(username + "@" + parent_path_dest, "items", json_dest, serialize_items(items_dest), username, master_servers[0]);
                // delete the file and upload it again with new name
                string file_content = table_get(username + "@" + path, "content", username, master_servers[0]);
                table_delete(username + "@" + path, "content", username, master_servers[0]);
                table_put(username + "@" + req_params["new_path"], "content", file_content, username, master_servers[0]);
                response = render_move_page(path, req_params["new_path"], username);
            }
        }
    } else if (req_page.find(delete_route) == 0) { // delete a file/folder
        int idx = path.rfind("/");
        string parent_path = path.substr(0, idx + 1);
        string file_name = path.substr(idx + 1);
        string get_result = table_get(username + "@" + parent_path, "items", username, master_servers[0]);
        if (get_result.find("-ERR") == 0) {
            response = render_error_page("Parent folder doesn't exist", username);
        } else {
            string json = get_result;
            if (debug) printf("Delete JSON: %s\n", json.c_str());
            vector<Item> items = deserialize_items(json);
            int j = find_item(items, file_name);

            if (file_name == "") { // delete a folder
                // response = render_error_page("Deleting folders hasn't been supported yet");
                if (!items.empty()) {
                    response = render_error_page("You have to delete all files inside the folder first", username);
                } else {
                    idx = path.rfind("/", path.length() - 2);
                    parent_path = path.substr(0, idx + 1);
                    string folder_name = path.substr(idx + 1, path.length() - idx - 2);
                    get_result = table_get(username + "@" + parent_path, "items", username, master_servers[0]);
                    if (get_result.find("-ERR") == 0) {
                        response = render_error_page("Parent folder doesn't exist", username);
                    } else {
                        // remove it from parent folder's item list
                        json = get_result;
                        items = deserialize_items(json);
                        j = find_item(items, folder_name);
                        items.erase(items.begin() + j);
                        table_cput(username + "@" + parent_path, "items", json, serialize_items(items), username, master_servers[0]);
                        // delete the (empty) folder
                        table_delete(username + "@" + path, "items", username, master_servers[0]);
                        response = render_delete_page(path, "", username);
                    }
                }
            } else if (j == -1) {
                response = render_error_page("File/folder doesn't exist", username);
            } else {
                // remove it from parent folder's item list
                items.erase(items.begin() + j);
                table_cput(username + "@" + parent_path, "items", json, serialize_items(items), username, master_servers[0]);
                // delete the file
                table_delete(username + "@" + path, "content", username, master_servers[0]);
                response = render_delete_page(path, "", username);
            }
        }
    }

    // send the response message
    write(comm_fd, response.c_str(), response.length());
    if (debug) fprintf(stderr, "[%d] S: %s", comm_fd, response.c_str());
    if (debug) fprintf(stderr, "[%d] Finishing current thread\n", comm_fd);
    thread_status[thread_id] = finished;
    close(comm_fd);
    // delete (int*)arg;
    // pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int opt;
    // int port = 8000;
    while ((opt = getopt(argc, argv, "p:av")) != -1) {
        if (opt == 'p') {
            port = atoi(optarg);
        } else if (opt == 'v') {
            debug = true;
        } else {
            fprintf(stderr, "[Drive] Unrecognized option -%c.\n", opt);
            exit(1);
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "[Drive] Please specify a configuration file.\n");
        exit(1);
    }
    char* config_file = argv[optind];

    // read the configuration file
    parse_config(config_file, master_servers, storage_servers, mail_servers, user_servers);
    // username = ""; password = "";

    running = true;
    signal(SIGINT, int_handler);
    signal(SIGPIPE, SIG_IGN); // temporary fix

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("[Drive] Socket");
        exit(1);
    }
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        perror("[Drive] Setsockopt");
        exit(1);
    }
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("[Drive] Bind");
        exit(1);
    }
    if (listen(listen_fd, 10) < 0) {
        perror("[Drive] Listen");
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
            if (debug) fprintf(stderr, "[Drive %d] New connection from %s\n", fd, inet_ntoa(clientaddr.sin_addr));

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

    if (debug) fprintf(stderr, "[Drive] Joining %lu threads...\n", threads.size());
    for (int i = 0; i < threads.size(); i++) {
        threads[i].join();
    }
    if (debug) fprintf(stderr, "[Drive] Closing the server connection...\n");
    close(listen_fd);
    return 0;
}
