/*
 * mail.hh
 *
 *  Created on: Nov 19, 2017
 *      Author: cis505
 */

#ifndef MAIL_HH_
#define MAIL_HH_

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <thread>
#include <mutex>
#include <iomanip>
#include <ctime>
#include <time.h>

#include <algorithm>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>

#include <resolv.h>

#include "keyvalue.hh"

#define PORT 9000
#define KEY_VALUE_STORE_PORT 2500

const int MAX_MSG_LEN = 1048576;
const int MAX_KEYWORD_LEN = 1048576;
const int MAX_USERNAME_LEN = 64;
const int MAX_DOMAIN_LEN = 64;
const int MAX_PATH_LEN = 256;
const int MAX_REPLYLINE_LEN = 512;
const int MAX_THREAD_NUM = 100;


const char HELLO_MSG[MAX_MSG_LEN] =
		"220 localhost Mail Transfer Service Ready\n";

const std::string LOCALHOST = "127.0.0.1";
// SMTP command
const char HELO[5] = "HELO";
const char MAIL[5] = "MAIL";
const char RCPT[5] = "RCPT";
const char MAIL_FROM[11] = "MAIL FROM:";
const char RCPT_TO[9] = "RCPT TO:";
const char DATA[5] = "DATA";
const char RSET[5] = "RSET";
const char NOOP[5] = "NOOP";
const char QUIT[5] = "QUIT";
const char ENDOFMAIL[2] = ".";

// From Master
const char SCHECK[7] = "/CHECK";

// From html server
const char SUSER[6] = "/USER";

// HTML command
const char POST[5] = "POST";
const char GET[4] = "GET";
const char COOKIEC[8] = "Cookie:";

bool OpenPage = true;
bool testing = false;
bool debug = true;
volatile bool running = true;

// For multithreading
std::mutex t_mutex;
int port = PORT, verbose = 0, verbose2 = 0;
int serverIdx = -1;
int v_shutdown = 0, new_socket;
std::vector<int> socket_vector;
int thread_id = 0, close_thread_id = 0, active_thread_num = 0;
bool thread_occupied[MAX_THREAD_NUM] = {false};
bool thread_finished[MAX_THREAD_NUM] = {false};
int addrlen, listen_fd;
std::string configFilename;

struct sockaddr_in clientaddr;
socklen_t clientaddrlen = sizeof(clientaddr);

// Store configuration file
std::vector<sockaddr_in> vec_masterServer;
std::vector<sockaddr_in> vec_storageServer;
std::vector<sockaddr_in> vec_mailServer;
std::vector<sockaddr_in> vec_userServer;

int keyValuePort = 2500;

struct sockaddr_in masterAddr;
socklen_t masterAddrLen = sizeof(masterAddr);

// For parsing request
std::string reqType = "";
std::string reqPage = "";
std::string reqProto = "";

// Store current user
std::string currentUser = "";
std::string currentUserMailAddr = "";
std::string currentPw = "";

// Some basic http response
const std::string TEXT_CSS = "Content-Type: text/css\r\n";
const std::string TEXT_HTML = "Content-Type: text/html\r\n";
const std::string CONTENT_LEN = "Content-length: ";
const std::string CONTENT_BEGIN = "\r\n\r\n";
const std::string OK_200 = " 200 OK\r\n";
const std::string ERR_404 = " 404 Not Found\r\n";
const std::string ERR_500 = " 500 Internal Server Error\r\n";
const std::string ERR_503 = " 503 Service Unavailable\r\n";

// css file for mail page
const std::string MAILBOX_CSS = ".btn{font-weight:lighter}a,a:hover,a:focus,a:active{text-decoration:none}body{font-size:13px}body{background:#ecf0f5;padding-top:50px;font-family:'Source Sans Pro', sans-serif}.mailbox-box{position:relative;background:white;border-radius:3px;box-shadow:0 1px 1px rgba(0,0,0,0.1);margin-bottom:20px}.mailbox-box-header{border-bottom:1px solid #f4f4f4;padding:10px}.mailbox-box-header .mailbox-box-title{font-size:16px;color:#444;margin:0;font-weight:lighter;display:inline-block}.mailbox-box-header .mailbox-box-tools>form{width:200px;margin:0;padding:0}.mailbox-box-header .mailbox-box-tools>form.search-form{margin-top:-5px;position:relative}.mailbox-box-header .mailbox-box-tools>form.search-form>input[type=text]{font-size:11px;box-shadow:none;border-radius:0;height:28px;padding-right:30px}.mailbox-box-header .mailbox-box-tools>form.search-form>i{position:absolute;top:8px;right:10px;color:#555}.mailbox-box-header .mailbox-box-tools>.btn{margin-top:-3px;padding:2px 5px;color:#97a0b3}.mailbox-box-footer{padding:10px 15px}.mailbox-box-primary{border-top:3px solid #3c8dbc}.mailbox .vertical-list{margin:0;padding:0}.mailbox .vertical-list>li{list-style:none;font-weight:lighter;border-bottom:1px solid #f4f4f4}.mailbox .vertical-list>li:hover{background:#f7f7f7}.mailbox .vertical-list>li:last-child{border-bottom:none}.mailbox .vertical-list>li.active{font-weight:normal}.mailbox .vertical-list>li a{display:block;padding:10px;color:#444}.mailbox .vertical-list>li a>i{margin-right:5px}.mailbox-controls{padding:10px;border-bottom:1px solid #f4f4f4}.mailbox-messages>.table{margin-bottom:0;border-bottom:1px solid #f4f4f4}.mailbox-messages>.table>tbody>tr:first-child>td{border-top:none}.mailbox-messages>.table>tbody>tr>td{border-top:1px solid #f4f4f4}.mailbox-messages>.table>tbody>tr>td:first-child{text-align:center}.mailbox-read-info{padding:10px;border-bottom:1px solid #f4f4f4}.mailbox-read-info h3{font-size:20px;margin:0;font-weight:lighter}.mailbox-read-info h5{margin:0;padding:5px 0 0 0;font-weight:lighter}.mailbox-read-info-time{font-size:13px;color:#999}.mailbox-read-message{padding:10px;font-weight:lighter;border-bottom:1px solid #f4f4f4}.mailbox-attachment{margin:0;padding:10px 5px 10px 5px;border-bottom:1px solid #f4f4f4}.mailbox-attachment>li{float:left;width:150px;border:1px solid #eee;margin-top:10px;margin-bottom:10px;list-style:none;margin:5px}.mailbox-attachment-name{color:#666;display:block;font-size:11px;font-weight:bold}.mailbox-attachment-size{color:#999;font-size:11px}.mailbox-attachment-icon{color:#666;padding:20px 10px;font-size:41.5px;text-align:center;display:block}.mailbox-attachment-icon.has-img{padding:0}.mailbox-attachment-icon.has-img>img{max-width:100%}.mailbox-attachment-info{background:#f4f4f4;padding:10px}.mailbox-compose{border-bottom:1px solid #f4f4f4;padding:10px}.mailbox-compose input[type=\"text\"].form-control,.mailbox-compose textarea.form-control,.mailbox-compose div.same-form-control{box-shadow:none;border-radius:0;font-weight:lighter;border:1px solid #d2d6de;padding:5px 10px;font-size:12px}.mailbox-compose input[type=\"text\"].form-control:focus,.mailbox-compose textarea.form-control:focus,.mailbox-compose div.same-form-control:focus{outline:none}.mailbox-compose .wysihtml5-toolbar{margin:0 0 10px 0;padding:0}.mailbox-compose .wysihtml5-toolbar>li{display:inline-block}.mailbox[dir=\"rtl\"] .vertical-list>li>a>i{margin-right:0;margin-left:5px}.mailbox[dir=\"rtl\"] .mailbox-box-tools>.search-form>input[type=text]{padding-right:10px;padding-left:30px}.mailbox[dir=\"rtl\"]"
		".mailbox-box-tools>.search-form>i{right:auto;left:10px}\n";

// For mail index page
const std::string INDEX_HTML_BEFORE_INBOX = "<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<link rel='icon' href='data:,'>\n"
"<link rel='stylesheet' type='text/css' href='mailbox.css'>\n"
"	<title>Mailbox</title>\n"
"</head>\n"
"<body>\n"
"	<div class='container mailbox'>\n"
"		<div class='col-md-3'>\n"
"			<div class='mailbox-box mailbox-box-solid'>\n"
"				<div class='mailbox-box-header'>\n"
"					<a href='http://localhost:10000' class='btn btn-primary btn-block'><strong>Log out</strong></a>\n"
"				</div>\n"
"				<div class='mailbox-box-header'>\n"
"					<a href='compose.html' class='btn btn-primary btn-block'><b>Compose</b></a>\n"
"				</div>\n"
"				<div class='mailbox-box-body'>\n"
"					<ul class='vertical-list'>\n"
"						<li class='active'><a href='mail_index.html'>\n"
"							<i class='fa fa-inbox'></i>\n"
"							Inbox\n"
"							<span class='label label-primary pull-right flip'>";

// For mail index page
const std::string INDEX_HTML_BEFORE_SENT =
"						</span>\n"
"						</a></li>\n"
"						<li><a href='mail_index.html'>\n"
"							<i class='fa fa-envelope-o'></i>\n"
"							Sent ";

// For mail index page
const std::string INDEX_HTML_BEFORE_TRASH =
"						</a></li>\n"
"						<li><a href='mail_index.html'>\n"
"							<i class='fa fa-trash'></i>\n"
"							Trash ";

// For mail index page
const std::string INDEX_HTML_AFTER_TRASH =
"						</a></li>\n"
"					</ul>\n"
"				</div>\n"
"			</div> <!-- End Box -->\n"
"		</div>\n"
"		<!-- Sidebar -->\n";

// For mail index page
const std::string INDEX_HTML_MAINCONTENT_HEADER =
"		<div class='col-md-9'>\n";

// For mail index page
const std::string INDEX_HTML_BEFORE_TABLE =
"					    </div>\n"
"					</div>\n"
"					<!-- Controls -->\n"
"					<div class='table-responsive mailbox-messages'>\n"
"					    <table class='table table-hover table-striped'>\n"
"					        <tbody>\n";

// For mail index page
const std::string INDEX_HTML_AFTER_TABLE =
"					        </tbody>\n"
"					    </table>\n"
"					</div> <!-- Messages -->\n"
"					<!-- Controls -->\n"
"				</div>\n"
"			</div> <!-- End Box -->\n"
"		</div>\n"
"		<!-- Content -->\n";

// For mail index page
const std::string INDEX_HTML_END =
"	</div>\n"
"</body>\n"
"<!-- Credited to Moein Porkamel -->\n"
"</html>\n";

// For reading mail page
const std::string READ_BEFORE_SUBJECT =
"		<div class='col-md-9'>\n"
"			<div class='mailbox-box mailbox-box-primary'>\n"
"				<div class='mailbox-box-header'>\n"
"					<h3 class='mailbox-box-title'>Read Mail</h3>\n"
"					<div class='mailbox-box-tools pull-right flip'>\n"
"						<a href='read.html' class='btn btn-sm'><i class='fa fa-chevron-left'></i></a>\n"
"						<a href='read.html' class='btn btn-sm'><i class='fa fa-chevron-right'></i></a>\n"
"					</div>\n"
"				</div>\n"
"				<div class='mailbox-box-body'>\n"
"					<div class='mailbox-read-info'>\n"
"					    <h3>\n";

// For reading mail page
const std::string READ_BEFORE_FROM =
"					    </h3>\n"
"					    <h5>\n";

// For reading mail page
const std::string READ_BEFORE_TIME =
"<span class='mailbox-read-info-time pull-right flip'>\n";

// For reading mail page
const std::string READ_BEFORE_MAINCONTENT =
"					        </span>\n"
"					    </h5>\n"
"					</div>\n"
"					<!-- Read Info -->\n"
"					<div class='mailbox-read-message'>\n";

// For reading mail page
const std::string READ_END =
"					</div> <!-- Read Message -->\n"
"				</div>\n"
"				<div class='mailbox-box-footer'>\n"
"				    <div class='pull-right flip'>\n"
"      					<form class='mailbox-compose' method='post'>\n"
"				        <button name='reply' type='submit' class='btn btn-default'>\n"
"				            Reply\n"
"				        </button>\n"
"				        <button name='forward' type='submit' class='btn btn-default'>\n"
"				            Forward\n"
"				        </button>\n"
"				   		<button name='delete' type='submit' class='btn btn-default'>\n"
"				        	Delete\n"
"				    	</button>\n"
"     			 		</form>\n"
"				    </div>\n"
"				</div>\n"
"			</div> <!-- End Box -->\n"
"		</div>\n"
"		<!-- Content -->\n"
"	</div>\n"
"</body>\n"
"</html>\n";

// For composing mail page
const std::string COMPOSE_BEFORE_FROM =
"<div class='col-md-9'>\n"
"  <div class='mailbox-box mailbox-box-primary'>\n"
"    <div class='mailbox-box-header'>\n"
"      <h3 class='mailbox-box-title'>Compose New Message</h3>\n"
"    </div>\n"
"    <div class='mailbox-box-body'>\n"
"      <form class='mailbox-compose' method='post'>\n"
"        <div class='form-group'>\n"
"            From: \n";

// For composing mail page
const std::string COMPOSE_AFTER_FROM =
"        </div>\n"
"        <div class='form-group'>\n"
"            <input name='to' class='form-control' type='text' placeholder='To:' />\n"
"        </div>\n"
"        <div class='form-group'>\n"
"            <input name='subject' class='form-control' type='text' placeholder='Subject:' />\n"
"        </div>\n"
"        <div class='form-group'>\n"
"            <textarea name='content' class='text-area' id='mail-content' rows='20' cols='50' ></textarea>\n"
"        </div>\n"
"            <button class='btn btn-primary' type='submit'>\n"
"                Send\n"
"            </button>\n"
"      </form>\n"
"    </div> <!-- Body -->\n"
"    <div class='mailbox-box-footer'>\n"
"        <div class='pull-right'>\n"
"        </div>\n"
"    </div> <!-- Footer -->\n"
"  </div> <!-- End Box -->\n"
"</div>\n"
"<!-- Content -->\n";

// Set max text length displayed in index page for each mail
const int INBOX_TABLE_ROW_LIMIT = 40;

struct Mail {
	std::string mailTime;
	std::string mailFrom;
	std::string mailTo;
	std::string mailSubject;
	std::string mailMainContent;
};

// Function declaration
void sigint(int a);
void ServerProgram(int new_socket);

inline std::string GetCurrentTime();
long int GetTimeDiffInSecond(std::string dateTime);
inline std::string GetPluralSuffix(int x);
std::string GetElapsedTimeStr(std::string dateTime);
struct Mail ParseMail(std::string mailStr,
		std::string currentUserMailAddr);
std::string GenerateTableRow(std::string mailAddr, std::string subject,
		std::string mainContent, std::string timeElapsed,
		std::string hrefName, int index, int port);
std::string GenerateTableRow(struct Mail mail, std::string hrefName, int index, int port);
inline std::string ParagraphMailContent(std::string mailContent);
std::string GetTableHeader(std::string tableTitle);
std::string GetSidebar(struct sockaddr_in keyValueAddr,
		std::string currentUser, int thread_socket);

bool AddNewMail(struct sockaddr_in keyValueAddr,
		std::string currentUser, int thread_socket, std::string command);
bool AddNewMail(struct sockaddr_in keyValueAddr,
		std::string currentUser, int thread_socket,
		std::string mailTo, std::string mailSubject, std::string mailMainContent);
void DeleteMail(struct sockaddr_in keyValueAddr,
		struct sockaddr_in masterAddr,
		std::string currentUser, int thread_socket,
		std::string mailID, struct Mail mail);
std::string GetIndexPageContent(struct sockaddr_in keyValueAddr,
		std::string currentUser, int thread_socket, std::string mailNum, int port);
inline std::string GetComposeAfterForm();
std::string GetComposeAfterForm(std::string mailTo,
		std::string mailSubject, std::string mailMainContent);
std::string GetComposeAlertPage(std::string alertMsg, int port);
std::string GetServerAddrByUsername(struct sockaddr_in masterAddr,
		std::string username, int thread_socket);
std::string SetCookieUsername(std::string username);

inline bool StringMatch(char* keyword, const char* pattern);
inline std::string SockAddr2Str(struct sockaddr_in servaddr);
inline struct sockaddr_in Str2SockAddr(std::string servaddrStr);
time_t Str2Time_t(std::string dateTime);
inline std::string url_decode(std::string text);
inline char from_hex(char ch);
inline std::string RemoveCR(std::string str);
inline std::string IncreStr(std::string intStr);
inline std::string IncreStr(std::string intStr, int i);
inline bool isInteger(const std::string &s);
std::vector<std::string> query_MX_record(std::string hostStr);
std::string parse_record (unsigned char *buffer, size_t r,
                   const char *section, ns_sect s,
                   int idx, ns_msg *m);
void clean(char *var);

std::vector<std::string> StrSplit(std::string strToSplit, const char delimiter);

std::string GetServerResponse
(struct sockaddr_in serverAddr, std::string request, int thread_socket);
std::string GetServerResponse(int serverSocket, std::string request);
void SendMessage(int socket, std::string message);
inline void OpenResponsePage(std::string responsePage);
inline void WriteFile(std::string fileName, std::string content);


// ----------------- Functions -----------------
std::string SetCookieUsername(std::string username) {
	/* Given username, send Set-cookie to brower */
	std::vector<std::string> tmp = StrSplit(username, '@');
	username = tmp[0] + "%40" + tmp[1];

	return "Set-Cookie: username=" + username + "\r\n";
}

std::string GetServerAddrByUsername(struct sockaddr_in masterAddr,
		std::string username, int thread_socket) {

	/* Ask master for in which port of keyvalue store the current user data are stored */
	if (verbose) std::cout << "[Finding kv ip for user]" << username << std::endl;
	if (verbose) std::cout << "[Send request to Master]" << std::endl;
	if (verbose) std::cout << "[masterAddr2]" << SockAddr2Str(masterAddr) << std::endl;
	std::string masterResponse = GetServerResponse(masterAddr,
			"/MAIL " + username.substr(0, 1) + " \r\n", thread_socket);

	while (masterResponse.size() <= 0) {
		// Wait for a non-empty response from master
		masterResponse = GetServerResponse(masterAddr,
			"/MAIL " + username.substr(0, 1) + " \r\n", thread_socket);
		if(verbose) std::cout << "[Master Response]" << masterResponse << std::endl;
		if(verbose) std::cout << "[Response length]" << masterResponse.size() << std::endl;
	}
	std::vector<std::string> vec_tmp2 = StrSplit(masterResponse, ' ');

	return vec_tmp2[1];
}

std::vector<std::string> query_MX_record(std::string hostStr) {

	/* Find ip for external mail server */
	std::vector<std::string> result;

    const size_t size = 1024;
    unsigned char buffer[size];
    const char *host = hostStr.c_str();

    int r = res_query(host, C_IN, T_MX, buffer, size);
    if (r == -1) {
        std::cerr << "Cannot find host record." << std::endl;
        return result;
    }
    else {
        if (r == static_cast<int>(size)) {
            std::cerr << "Buffer too small reply truncated\n";
            return result;
        }
    }

    HEADER *hdr = reinterpret_cast<HEADER*>(buffer);

	if (hdr->rcode != NOERROR) {

		std::cerr << "Error: ";
		switch(hdr->rcode) {
		case FORMERR:
		 std::cerr << "Format error";
		 break;
		case SERVFAIL:
		 std::cerr << "Server failure";
		 break;
		case NXDOMAIN:
		 std::cerr << "Name error";
		 break;
		case NOTIMP:
		 std::cerr << "Not implemented";
		 break;
		case REFUSED:
		 std::cerr << "Refused";
		 break;
		default:
		 std::cerr << "Unknown error";
		}
		return result;
	}

    int question = ntohs(hdr->qdcount);
    int answers = ntohs(hdr->ancount);
    int nameservers = ntohs(hdr->nscount);
    int addrrecords = ntohs(hdr->arcount);

    if (verbose) std::cout << "Reply: question: " << question << ", answers: " << answers
              << ", nameservers: " << nameservers
              << ", address records: " << addrrecords << "\n";

    ns_msg m;
    int k = ns_initparse (buffer, r, &m);
    if (k == -1) {
        std::cerr << errno << " " << strerror(errno) << "\n";
        return result;
    }


    if (0) {
		for (int i = 0; i < question; ++i) {
			ns_rr rr;
			int k = ns_parserr(&m, ns_s_qd, i, &rr);
			if (k == -1) {
				std::cerr << errno << " " << strerror(errno) << "\n";
				return result;
			}
			if(verbose) std::cout << "question " << ns_rr_name(rr) << " "
					  << ns_rr_type(rr) << " " << ns_rr_class(rr) << "\n";
		}
    }
    if (1) {
        for (int i = 0; i < answers; ++i) {
        	parse_record(buffer, r, "answers", ns_s_an, i, &m);
        }
    }
    if (1) {
        for (int i = 0; i < nameservers; ++i) {
            parse_record(buffer, r, "nameservers", ns_s_ns, i, &m);
        }
    }
    if (1) {
        for (int i = 0; i < addrrecords; ++i) {
        	result.push_back(parse_record(buffer, r, "addrrecords", ns_s_ar, i, &m));
        }
    }

    if(verbose) std::cout << "IPs: " << std::endl;
    for (auto record : result) {
    	std::cout << record << std::endl;
    }

    if(verbose) std::cout << "=======================================================" << std::endl;
    return result;
}


std::string parse_record (unsigned char *buffer, size_t r,
                   const char *section, ns_sect s,
                   int idx, ns_msg *m) {

	/* Parse MX record */
    ns_rr rr;
    int k = ns_parserr(m, s, idx, &rr);
    if (k == -1) {
        std::cerr << errno << " " << strerror(errno) << "\n";
        return "";
    }


    std::cout << section << " " << ns_rr_name(rr) << " "
              << ns_rr_type(rr) << " " << ns_rr_class(rr)
              << ns_rr_ttl(rr) << " ";

    const size_t size = NS_MAXDNAME;
    unsigned char name[size];
    int t = ns_rr_type(rr);

    std::string ip;

    const u_char *data = ns_rr_rdata(rr);
    if (t == T_MX) {
        int pref = ns_get16(data);
        ns_name_unpack(buffer, buffer + r, data + sizeof(u_int16_t),
                        name, size);
        char name2[size];
        ns_name_ntop(name, name2, size);
        if(verbose) std::cout << "!";
        if(verbose) std::cout << pref << " " << name2;
    }
    else if (t == T_A) {
        unsigned int addr = ns_get32(data);
        struct in_addr in;
        in.s_addr = ntohl(addr);
        char *a = inet_ntoa(in);
        if(verbose) std::cout << "@";
        ip = a;
        if(verbose) std::cout << ip;
    }
    else if (t == T_NS) {
        ns_name_unpack(buffer, buffer + r, data, name, size);
        char name2[size];
        ns_name_ntop(name, name2, size);
        if(verbose) std::cout << "#";
        if(verbose) std::cout << name2;
    }
    else {
    	if(verbose) std::cout << "unhandled record";
    }

    if(verbose) std::cout << "\n";

    return ip;
}

std::string GetComposeAlertPage(std::string alertMsg, int port) {

	/* If the composed mail cannot be sent, return an error message */
	std::string alertPage = "<!DOCTYPE html><html>"
					"<a href='http://localhost:" +
					std::to_string(port) + "/compose.html'>"
					"<b>" + alertMsg + "</b></a</html>";

	return alertPage;

}


inline bool isInteger(const std::string &s) {

	/* Test whether the string is integer */
	if(s.empty() ||
	   ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) {
	   return false;
	}

	char *p;
	strtol(s.c_str(), &p, 10);

	return (*p == 0);
}


std::string GetTableHeader(std::string tableTitle) {

	/* Return table header with given table name */
	std::string header =
	"			<div class='mailbox-box mailbox-box-primary'>\n"
	"				<div class='mailbox-box-header'>\n"
	"					<h3 class='mailbox-box-title'>" +
	tableTitle +
	"					</h3>\n"
	"				</div>\n"
	"				<div class='mailbox-box-body'>\n"
	"					<div class='mailbox-controls'>\n"
	"					    <div class='pull-right flip'>\n";

	return header;
}

inline std::string GetComposeAfterForm() {
	/* Return empty compose forms */
	return COMPOSE_AFTER_FROM;
}

std::string GetComposeAfterForm(std::string mailTo,
		std::string mailSubject, std::string mailMainContent) {

	/* Return the compose forms with saved content */
	std::string afterForm =
	"        </div>\n"
	"        <div class='form-group'>\n"
	"            <input name='to' class='form-control' value='" +
	mailTo +
	"'type='text' placeholder='To:' />\n"
	"        </div>\n"
	"        <div class='form-group'>\n"
	"            <input name='subject' class='form-control' value='" +
	mailSubject +
	"'type='text' placeholder='Subject:' />\n"
	"        </div>\n"
	"        <div class='form-group'>\n"
	"            <textarea name='content' class='text-area' id='mail-content' rows='20' cols='50' >" +
	mailMainContent +
	"</textarea>\n"
	"        </div>\n"
	"            <button class='btn btn-primary' type='submit'>\n"
	"                Send\n"
	"            </button>\n"
	"      </form>\n"
	"    </div> <!-- Body -->\n"
	"    <div class='mailbox-box-footer'>\n"
	"        <div class='pull-right'>\n"
	"        </div>\n"
	"    </div> <!-- Footer -->\n"
	"  </div> <!-- End Box -->\n"
	"</div>\n"
	"<!-- Content -->\n";

	return afterForm;
}


inline std::string RemoveCR(std::string str) {
	/* Remove \r\n in string */
	str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
	str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
	return str;
}

std::string GetSidebar(struct sockaddr_in keyValueAddr,
		std::string currentUser, int thread_socket) {

	/* Get the sidebar on the top of a page */
	std::string mailNum = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " mailNum\r\n", thread_socket);
	std::string sentNum = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " sentNum\r\n", thread_socket);
	std::string trashNum = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " trashNum\r\n", thread_socket);

	if (verbose) std::cout << "[mailNum]" << mailNum << std::endl;
	if (verbose) std::cout << "[sentNum]" << sentNum << std::endl;
	if (verbose) std::cout << "[trashNum]" << trashNum << std::endl;

	std::string sidebar = INDEX_HTML_BEFORE_INBOX + mailNum +
				INDEX_HTML_BEFORE_SENT + sentNum +
				INDEX_HTML_BEFORE_TRASH + trashNum +
				INDEX_HTML_AFTER_TRASH;

	return sidebar;
}

std::string GetIndexPageContent(struct sockaddr_in keyValueAddr,
		std::string currentUser, int thread_socket, int port) {

	/* Get the index page for current user */
	if (currentUser == "") {

		// If the user is not logged in, redirect it to the login page
		return "<!DOCTYPE html><html>"
				"<a href='http://localhost:10000'>"
				"<b>Please Login.</b></a</html>";
	}

	// Get user mailbox data
	std::string reqKV;
	std::string mailNum = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " mailNum\r\n", thread_socket);
	std::string mailIdxMax = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " mailIdxMax\r\n", thread_socket);
	std::string sentNum = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " sentNum\r\n", thread_socket);
	std::string sentIdxMax = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " sentIdxMax\r\n", thread_socket);
	std::string trashNum = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " trashNum\r\n", thread_socket);
	std::string trashIdxMax = GetServerResponse(
			keyValueAddr, "/GET " + currentUser + " trashIdxMax\r\n", thread_socket);

	// If get -ERR from key value store, initialize the user mail account
	if (mailNum.substr(0, 4) == "-ERR") {
		GetServerResponse(
			keyValueAddr, "/PUT " + currentUser + " mailNum 0\r\n", thread_socket);
		mailNum = "0";
	}
	if (mailIdxMax.substr(0, 4) == "-ERR") {
		GetServerResponse(
			keyValueAddr, "/PUT " + currentUser + " mailIdxMax 0\r\n", thread_socket);
		mailIdxMax = "0";
	}
	if (sentNum.substr(0, 4) == "-ERR") {
		GetServerResponse(
			keyValueAddr, "/PUT " + currentUser + " sentNum 0\r\n", thread_socket);
		sentNum = "0";
	}
	if (sentIdxMax.substr(0, 4) == "-ERR") {
		GetServerResponse(
			keyValueAddr, "/PUT " + currentUser + " sentIdxMax 0\r\n", thread_socket);
		sentIdxMax = "0";
	}
	if (trashNum.substr(0, 4) == "-ERR") {
		GetServerResponse(
			keyValueAddr, "/PUT " + currentUser + " trashNum 0\r\n", thread_socket);
		trashNum = "0";
	}
	if (trashIdxMax.substr(0, 4) == "-ERR") {
		GetServerResponse(
			keyValueAddr, "/PUT " + currentUser + " trashIdxMax 0\r\n", thread_socket);
		trashIdxMax = "0";
	}


	std::string content = GetSidebar(keyValueAddr, currentUser, thread_socket) +
			INDEX_HTML_MAINCONTENT_HEADER +
			GetTableHeader("Inbox") +
			mailNum + " Message" +
			GetPluralSuffix(std::stoi(mailNum)) +
			INDEX_HTML_BEFORE_TABLE;

	// Display emails in tables
	for (int i = 1; i <= std::stoi(mailIdxMax); ++i) {

		reqKV = "/GET " + currentUser + " mail" + std::to_string(i) + "\r\n";
		std::string mailContent = GetServerResponse(
				keyValueAddr, reqKV, thread_socket);
		if (mailContent.substr(0, 4) != "-ERR") {
			mailContent = decode_base64(mailContent);
			struct Mail mail = ParseMail(mailContent, currentUser);
			content += GenerateTableRow(mail, "mail", i, port);
		}
	}
	content += INDEX_HTML_AFTER_TABLE;
	content += GetTableHeader("Sent") +
			sentNum + " Message" +
			GetPluralSuffix(std::stoi(sentNum)) +
			INDEX_HTML_BEFORE_TABLE;

	for (int i = 1; i <= std::stoi(sentIdxMax); ++i) {

		reqKV = "/GET " + currentUser + " sent" + std::to_string(i) + "\r\n";
		std::string sentContent = GetServerResponse(
				keyValueAddr, reqKV, thread_socket);
		if (sentContent.substr(0, 4) != "-ERR") {
			sentContent = decode_base64(sentContent);
			struct Mail mail = ParseMail(sentContent, currentUser);
			content += GenerateTableRow(mail, "sent", i, port);
		}
	}
	content += INDEX_HTML_AFTER_TABLE;

	content += GetTableHeader("Trash") +
			trashNum + " Message" +
			GetPluralSuffix(std::stoi(trashNum)) +
			INDEX_HTML_BEFORE_TABLE;

	for (int i = 1; i <= std::stoi(trashIdxMax); ++i) {

		reqKV = "/GET " + currentUser + " trash" + std::to_string(i) + "\r\n";
		std::string trashContent = GetServerResponse(
				keyValueAddr, reqKV, thread_socket);
		if (trashContent.substr(0, 4) != "-ERR") {
			trashContent = decode_base64(trashContent);
			struct Mail mail = ParseMail(trashContent, currentUser);
			content += GenerateTableRow(mail, "trash", i, port);
		}
	}
	content += INDEX_HTML_AFTER_TABLE;
	content += INDEX_HTML_END;
	return content;
}

inline std::string IncreStr(std::string intStr, int i) {

	/* Add i string and return a string, default i = 1 */
	if (intStr.substr(0, 4) == "-ERR") {
		intStr = "0";
	}
	return std::to_string(std::stoi(intStr) + i);
}

inline std::string IncreStr(std::string intStr) {
	return IncreStr(intStr, 1);
}

void DeleteMail(struct sockaddr_in keyValueAddr,
		struct sockaddr_in masterAddr,
		std::string currentUser, int thread_socket,
		std::string mailID, struct Mail mail) {

	/* Delete email, if it is in inbox or sent, move it to trash, else, remove it from key value store */
	std::string mailType = "trash";
	if (mailID.substr(0, 4) == "mail") {
		mailType = "mail";
	} else if (mailID.substr(0, 4) == "sent") {
		mailType = "sent";
	} else if (mailID.substr(0, 5) == "trash") {
		mailType = "trash";
	}

	std::string idxMax = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " " + mailType + "IdxMax\r\n", thread_socket));
	std::string num = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " " + mailType + "Num\r\n", thread_socket));
	std::string trashIdxMax = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " trashIdxMax\r\n", thread_socket));
	std::string trashNum = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " trashNum\r\n", thread_socket));

	if (verbose) std::cout << "[idxMax]" << idxMax << std::endl;
	if (verbose) std::cout << "[num]" << num << std::endl;
	if (verbose) std::cout << "[trashIdxMax]" << trashIdxMax << std::endl;
	if (verbose) std::cout << "[trashNum]" << trashNum << std::endl;

	// Compress mail into a structured string
	std::string storeTrash = "";
	if (mail.mailTo == currentUser) {
		storeTrash = mail.mailTime + "|" +
				mail.mailFrom + ">" +
				mail.mailSubject + "\t" +
				mail.mailMainContent;
	} else if (mail.mailFrom == currentUser) {
		storeTrash = mail.mailTime + "|" +
				mail.mailTo + "<" +
				mail.mailSubject + "\t" +
				mail.mailMainContent;
	}
	storeTrash = encode_base64(storeTrash);

	if (mailType != "trash") {
		GetServerResponse(keyValueAddr,
				"/DELETE " + currentUser + " trashIdxMax\r\n", thread_socket);
		GetServerResponse(keyValueAddr,
				"/PUT " + currentUser + " trashIdxMax " +
				IncreStr(trashIdxMax) + "\r\n", thread_socket);
		GetServerResponse(keyValueAddr,
				"/DELETE " + currentUser + " trashNum\r\n", thread_socket);
		GetServerResponse(keyValueAddr,
				"/PUT " + currentUser + " trashNum " +
				IncreStr(trashNum) + "\r\n", thread_socket);
		GetServerResponse(keyValueAddr,
				"/PUT " + currentUser + " trash" +
				IncreStr(trashIdxMax) + " " + storeTrash + "\r\n",thread_socket);
	}
	keyValueAddr = Str2SockAddr(GetServerAddrByUsername(masterAddr, currentUser, thread_socket));

	GetServerResponse(keyValueAddr,
			"/DELETE " + currentUser + " " + mailType + "Num\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + currentUser + " " + mailType + "Num " +
			IncreStr(num, -1) + "\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/DELETE " + currentUser + " " + mailID + "\r\n", thread_socket);

}
bool AddNewMail(struct sockaddr_in keyValueAddr, struct sockaddr_in masterAddr,
		std::string currentUser, int thread_socket,
		std::string mailTo, std::string mailSubject, std::string mailMainContent, int aux) {

	/* If serverIdx is given, i.e. no need to ask master for key value store port */
	if (verbose) std::cout << "[auxDmailTo]" << mailTo << std::endl;
	if (verbose) std::cout << "[DmailSubject]" << mailSubject << std::endl;
	if (verbose) std::cout << "[DmailMainContent]" << mailMainContent << std::endl;

	std::vector<std::string> tmp = StrSplit(mailTo, '@');
	std::string mailToName = tmp[0];
	std::string mailToHost = tmp[1];
	if (verbose) std::cout << "[mailToName]" << mailToName << std::endl;
	if (verbose) std::cout << "[mailToHost]" << mailToHost << std::endl;

	// Get and process user account data
	std::string mailIdxMax = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + mailTo + " mailIdxMax\r\n", thread_socket));
	std::string mailNum = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + mailTo + " mailNum\r\n", thread_socket));
	std::string sentIdxMax = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " sentIdxMax\r\n", thread_socket));
	std::string sentNum = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " sentNum\r\n", thread_socket));

	if (verbose) std::cout << "[============]" << std::endl;
	if (verbose) std::cout << "[mailIdxMax]" << mailIdxMax << std::endl;
	if (verbose) std::cout << "[mailNum]" << mailNum << std::endl;
	if (verbose) std::cout << "[sentIdxMax]" << sentIdxMax << std::endl;
	if (verbose) std::cout << "[sentNum]" << sentNum << std::endl;
	if (verbose) std::cout << "[======-======]" << std::endl;

	if (mailNum.size() >= 4) {
		if (verbose) std::cout << "[======1======]" << std::endl;
		if (mailNum.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======1b======]" << std::endl;
			GetServerResponse(
				keyValueAddr, "/PUT " + mailTo + " mailNum 0\r\n", thread_socket);
			mailNum = "0";
		}
	} else if (!isInteger(mailNum.substr(mailNum.size() - 1, 1))) {
		if (verbose) std::cout << "[======1c======]" << std::endl;
		mailNum = mailNum.substr(0, mailNum.size() - 1);
	}

	if (mailIdxMax.size() >= 4) {
		if (verbose) std::cout << "[======2======]" << std::endl;
		if (mailIdxMax.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======2b======]" << std::endl;
			GetServerResponse(
				keyValueAddr, "/PUT " + mailTo + " mailIdxMax 0\r\n", thread_socket);
			mailIdxMax = "0";
		}
	} else if (!isInteger(mailIdxMax.substr(mailIdxMax.size() - 1, 1))) {
		if (verbose) std::cout << "[======2c======]" << std::endl;
		mailIdxMax = mailIdxMax.substr(0, mailIdxMax.size() - 1);
	}

	if (sentNum.size() >= 4) {
		if (verbose) std::cout << "[======3======]" << std::endl;
		if (sentNum.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======3b======]" << std::endl;
			GetServerResponse(
				keyValueAddr, "/PUT " + currentUser + " sentNum 0\r\n", thread_socket);
			sentNum = "0";
		}
	} else if (!isInteger(sentNum.substr(sentNum.size() - 1, 1))) {
		if (verbose) std::cout << "[======3c======]" << std::endl;
		sentNum = sentNum.substr(0, sentNum.size() - 1);
	}

	if (sentIdxMax.size() >= 4) {
		if (verbose) std::cout << "[======4======]" << std::endl;
		if (sentIdxMax.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======4b======]" << std::endl;
			GetServerResponse(
				keyValueAddr, "/PUT " + currentUser + " sentIdxMax 0\r\n", thread_socket);
			sentIdxMax = "0";
		}
	} else if (!isInteger(sentIdxMax.substr(sentIdxMax.size() - 1, 1))) {
		if (verbose) std::cout << "[======4c======]" << std::endl;
		sentIdxMax = sentIdxMax.substr(0, sentIdxMax.size() - 1);
	}

	if (verbose) std::cout << "[======-======]" << std::endl;
	if (verbose) std::cout << "[mailIdxMax2]" << mailIdxMax.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[mailNum2]" << mailNum.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[sentIdxMax2]" << sentIdxMax.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[sentNum2]" << sentNum.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[============]" << std::endl;

	// Compress mail into a structured string
	std::string storeMail = GetCurrentTime() + "|" +
			currentUser + ">" +
			url_decode(mailSubject) + "\t" +
			url_decode(mailMainContent);
	storeMail = encode_base64(storeMail);
	std::string storeSent = GetCurrentTime() + "|" +
			mailTo + "<" +
			url_decode(mailSubject) + "\t" +
			url_decode(mailMainContent);
	storeSent = encode_base64(storeSent);

	GetServerResponse(keyValueAddr,
			"/DELETE " + mailTo + " mailIdxMax\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + mailTo + " mailIdxMax " +
			IncreStr(mailIdxMax) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddr,
			"/DELETE " + mailTo + " mailNum\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + mailTo + " mailNum " +
			IncreStr(mailNum) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddr,
			"/DELETE " + currentUser + " sentIdxMax\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + currentUser + " sentIdxMax " +
			IncreStr(sentIdxMax) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddr,
			"/DELETE " + currentUser + " sentNum\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + currentUser + " sentNum " +
			IncreStr(sentNum) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddr,
			"/PUT " + mailTo + " mail" +
			IncreStr(mailIdxMax) + " " + storeMail + "\r\n",thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + currentUser + " sent" +
			IncreStr(sentIdxMax) + " " + storeSent + "\r\n",thread_socket);

	return true;
}


bool AddNewMail(struct sockaddr_in keyValueAddr, struct sockaddr_in masterAddr,
		std::string currentUser, int thread_socket,
		std::string mailTo, std::string mailSubject, std::string mailMainContent) {

	/* Add new mail to both sender's sent and receiver's inbox, if to penncloud user
	 * Add new mail to receiver's inbox, if to external server
	 */
	if (verbose) std::cout << "[DmailTo]" << mailTo << std::endl;
	if (verbose) std::cout << "[DmailSubject]" << mailSubject << std::endl;
	if (verbose) std::cout << "[DmailMainContent]" << mailMainContent << std::endl;

	std::vector<std::string> tmp = StrSplit(mailTo, '@');
	std::string mailToName = tmp[0];
	std::string mailToHost = tmp[1];
	if (verbose) std::cout << "[mailToName]" << mailToName << std::endl;
	if (verbose) std::cout << "[mailToHost]" << mailToHost << std::endl;

	if (mailToHost != "penncloud.com") {

		// If to external server, ask for ip
		std::vector<std::string> IPStrVec = query_MX_record(mailToHost);

		int idx = 2;
		if (IPStrVec.size() == 0) {
			return false;
		}
		if (IPStrVec.size() <= 2) {
			idx = 0;
		}


		if (verbose) std::cout << "[UseIP]" << IPStrVec[0] << std::endl;

		// Create connection with external server
		struct sockaddr_in SMTPAddr;
		socklen_t SMTPAddrLen = sizeof(SMTPAddr);

		bzero((char *) &SMTPAddr, sizeof(SMTPAddr));
		SMTPAddr.sin_addr.s_addr = inet_addr(IPStrVec[idx].c_str());
		SMTPAddr.sin_family = AF_INET;
		SMTPAddr.sin_port = htons(25);
		if(verbose) std::cout << "[SMTPAddr]" << SockAddr2Str(SMTPAddr) << std::endl;


		int newSocket;
	    if ((newSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
	        perror("Cannot establish connection with key-value store");
	        close(thread_socket);
	        if(verbose) printf("Socket %d closed.\n", thread_socket);
	        exit(EXIT_FAILURE);
	    }

	    if (connect(newSocket,
	    		(struct sockaddr*)&SMTPAddr, sizeof(SMTPAddr)) < 0) {
	    	perror("Cannot connect to key-value store");
	        close(thread_socket);
	        if(verbose) printf("Socket %d closed.\n", thread_socket);
			exit(EXIT_FAILURE);
	    }

	    std::string initMsg;
		char buffer[MAX_MSG_LEN] = {0};

		if (read(newSocket, buffer, MAX_MSG_LEN) == -1) {
			fprintf(stderr, "Fail to read server response.\n");
		} else {
			initMsg = buffer;
		}
		if (verbose) std::cout << "[initMsg]" << initMsg << std::endl;

		// Only after receiving greeting message from external mail server and wait for some time,
		// Begin to send mail to it, to avoid to be considered as spam and rejected
	    sleep(1);
	    if (verbose) std::cout << "[Begin] =================" << std::endl;

	    // Following SMTP protocol, send email
		GetServerResponse(newSocket, "HELO penncloud\r\n");
		GetServerResponse(newSocket, "MAIL FROM:<" + currentUser + ">\r\n");
		GetServerResponse(newSocket, "RCPT TO:<" + mailTo + ">\r\n");
		GetServerResponse(newSocket, "DATA\r\n");
		SendMessage(newSocket, "Subject: " + mailSubject + "\r\n\r\n" + mailMainContent);
		GetServerResponse(newSocket, "\r\n.\r\n");
		SendMessage(newSocket, "QUIT\r\n");

		if (verbose) std::cout << "================= SMTP sent =================" << std::endl;
	}

	// Get receiver's key value store port
	struct sockaddr_in keyValueAddrReceiver = Str2SockAddr(
			GetServerAddrByUsername(masterAddr, mailTo, thread_socket));
	if (verbose) std::cout << "[keyValueAddrReceiver]" << SockAddr2Str(keyValueAddrReceiver) << std::endl;

	// Get and process mail account data
	std::string mailIdxMax = RemoveCR(GetServerResponse(
			keyValueAddrReceiver,"/GET " + mailTo + " mailIdxMax\r\n", thread_socket));
	std::string mailNum = RemoveCR(GetServerResponse(
			keyValueAddrReceiver,"/GET " + mailTo + " mailNum\r\n", thread_socket));
	std::string sentIdxMax = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " sentIdxMax\r\n", thread_socket));
	std::string sentNum = RemoveCR(GetServerResponse(
			keyValueAddr,"/GET " + currentUser + " sentNum\r\n", thread_socket));

	if (verbose) std::cout << "[============]" << std::endl;
	if (verbose) std::cout << "[mailIdxMax]" << mailIdxMax << std::endl;
	if (verbose) std::cout << "[mailNum]" << mailNum << std::endl;
	if (verbose) std::cout << "[sentIdxMax]" << sentIdxMax << std::endl;
	if (verbose) std::cout << "[sentNum]" << sentNum << std::endl;
	if (verbose) std::cout << "[======-======]" << std::endl;

	if (mailNum.size() >= 4) {
		if (verbose) std::cout << "[======1======]" << std::endl;
		if (mailNum.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======1b======]" << std::endl;
			GetServerResponse(
				keyValueAddrReceiver, "/PUT " + mailTo + " mailNum 0\r\n", thread_socket);
			mailNum = "0";
		}
	} else if (!isInteger(mailNum.substr(mailNum.size() - 1, 1))) {
		if (verbose) std::cout << "[======1c======]" << std::endl;
		mailNum = mailNum.substr(0, mailNum.size() - 1);
	}

	if (mailIdxMax.size() >= 4) {
		if (verbose) std::cout << "[======2======]" << std::endl;
		if (mailIdxMax.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======2b======]" << std::endl;
			GetServerResponse(
				keyValueAddrReceiver, "/PUT " + mailTo + " mailIdxMax 0\r\n", thread_socket);
			mailIdxMax = "0";
		}
	} else if (!isInteger(mailIdxMax.substr(mailIdxMax.size() - 1, 1))) {
		if (verbose) std::cout << "[======2c======]" << std::endl;
		mailIdxMax = mailIdxMax.substr(0, mailIdxMax.size() - 1);
	}

	if (sentNum.size() >= 4) {
		if (verbose) std::cout << "[======3======]" << std::endl;
		if (sentNum.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======3b======]" << std::endl;
			GetServerResponse(
				keyValueAddr, "/PUT " + currentUser + " sentNum 0\r\n", thread_socket);
			sentNum = "0";
		}
	} else if (!isInteger(sentNum.substr(sentNum.size() - 1, 1))) {
		if (verbose) std::cout << "[======3c======]" << std::endl;
		sentNum = sentNum.substr(0, sentNum.size() - 1);
	}

	if (sentIdxMax.size() >= 4) {
		if (verbose) std::cout << "[======4======]" << std::endl;
		if (sentIdxMax.substr(0, 4) == "-ERR") {
			if (verbose) std::cout << "[======4b======]" << std::endl;
			GetServerResponse(
				keyValueAddr, "/PUT " + currentUser + " sentIdxMax 0\r\n", thread_socket);
			sentIdxMax = "0";
		}
	} else if (!isInteger(sentIdxMax.substr(sentIdxMax.size() - 1, 1))) {
		if (verbose) std::cout << "[======4c======]" << std::endl;
		sentIdxMax = sentIdxMax.substr(0, sentIdxMax.size() - 1);
	}

	if (verbose) std::cout << "[======-======]" << std::endl;
	if (verbose) std::cout << "[mailIdxMax2]" << mailIdxMax.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[mailNum2]" << mailNum.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[sentIdxMax2]" << sentIdxMax.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[sentNum2]" << sentNum.substr(0, 4) << std::endl;
	if (verbose) std::cout << "[============]" << std::endl;

	// Compress mail into a structured string
	std::string storeMail = GetCurrentTime() + "|" +
			currentUser + ">" +
			url_decode(mailSubject) + "\t" +
			url_decode(mailMainContent);
	storeMail = encode_base64(storeMail);
	std::string storeSent = GetCurrentTime() + "|" +
			mailTo + "<" +
			url_decode(mailSubject) + "\t" +
			url_decode(mailMainContent);
	storeSent = encode_base64(storeSent);

	// Put them into key value store
	GetServerResponse(keyValueAddrReceiver,
			"/DELETE " + mailTo + " mailIdxMax\r\n", thread_socket);
	GetServerResponse(keyValueAddrReceiver,
			"/PUT " + mailTo + " mailIdxMax " +
			IncreStr(mailIdxMax) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddrReceiver,
			"/DELETE " + mailTo + " mailNum\r\n", thread_socket);
	GetServerResponse(keyValueAddrReceiver,
			"/PUT " + mailTo + " mailNum " +
			IncreStr(mailNum) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddr,
			"/DELETE " + currentUser + " sentIdxMax\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + currentUser + " sentIdxMax " +
			IncreStr(sentIdxMax) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddr,
			"/DELETE " + currentUser + " sentNum\r\n", thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + currentUser + " sentNum " +
			IncreStr(sentNum) + "\r\n", thread_socket);

	GetServerResponse(keyValueAddrReceiver,
			"/PUT " + mailTo + " mail" +
			IncreStr(mailIdxMax) + " " + storeMail + "\r\n",thread_socket);
	GetServerResponse(keyValueAddr,
			"/PUT " + currentUser + " sent" +
			IncreStr(sentIdxMax) + " " + storeSent + "\r\n",thread_socket);

	return true;
}

bool AddNewMail(struct sockaddr_in keyValueAddr,
		struct sockaddr_in masterAddr,
		std::string currentUser, int thread_socket, std::string command) {

	// If the command is not parsed, first parse it
	std::vector<std::string> paramsPOST = StrSplit(command, '&');
	if (verbose) {
		std::cout << "Composed Mail: " << std::endl;
		for (auto param : paramsPOST) {
			std::cout << param << std::endl;
		}
	}

	std::string mailTo = url_decode(StrSplit(paramsPOST[0], '=')[1]);
	std::string mailSubject = url_decode(StrSplit(paramsPOST[1], '=')[1]);
	std::string mailMainContent = url_decode(StrSplit(paramsPOST[2], '=')[1]);
	mailMainContent.erase(std::remove(mailMainContent.begin(),
			mailMainContent.end(), '\r'), mailMainContent.end());

	return AddNewMail(keyValueAddr, masterAddr, currentUser, thread_socket, mailTo, mailSubject, mailMainContent);
}

inline char from_hex(char ch) {

	/* Auxiliary function for decoding url */
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

inline std::string url_decode(std::string text) {

	/* Decode url strings */
    char h;
    std::ostringstream escaped;
    escaped.fill('0');

    for (auto i = text.begin(), n = text.end(); i != n; ++i) {
    	std::string::value_type c = (*i);

        if (c == '%') {
            if (i[1] && i[2]) {
                h = from_hex(i[1]) << 4 | from_hex(i[2]);
                escaped << h;
                i += 2;
            }
        } else if (c == '+') {
            escaped << ' ';
        } else {
            escaped << c;
        }
    }

    return escaped.str();
}

inline std::string ParagraphMailContent(std::string mailContent) {

	/* Support for multiple paragrahs for mail */
	std::vector<std::string> tmpVec_str =
			StrSplit(mailContent, '\n');
	std::string paraMailContent = "";

	for (auto para : tmpVec_str) {
		paraMailContent += "<p>" + para + "</p>";
	}

	return paraMailContent;
}


struct Mail ParseMail(std::string mailStr,
		std::string currentUserMailAddr) {

	/* Parse a structured string for a mail */
	struct Mail mail;
	std::vector<std::string> tmpVec_str =
			StrSplit(mailStr, '|');
	mail.mailTime = tmpVec_str[0];

	if (tmpVec_str[1].find('>') != -1) {
		tmpVec_str = StrSplit(tmpVec_str[1], '>');
		mail.mailFrom = tmpVec_str[0];
		mail.mailTo = currentUserMailAddr;
	}
	if (tmpVec_str[1].find('<') != -1) {
		tmpVec_str = StrSplit(tmpVec_str[1], '<');
		mail.mailTo = tmpVec_str[0];
		mail.mailFrom = currentUserMailAddr;
	}

	tmpVec_str = StrSplit(tmpVec_str[1], '\t');
	mail.mailSubject = tmpVec_str[0];
	mail.mailMainContent = tmpVec_str[1];

	return mail;
}

std::string GenerateTableRow(struct Mail mail, std::string hrefName, int index, int port) {

	/* Given a mail, show it as a table row */
	std::string mailAddr;
	if (hrefName == "sent") {
		mailAddr = mail.mailTo;
	} else if (hrefName == "mail") {
		mailAddr = mail.mailFrom;
	} else if (hrefName == "trash") {
		mailAddr = mail.mailFrom;
	}
	return GenerateTableRow(mailAddr, mail.mailSubject,
			mail.mailMainContent, GetElapsedTimeStr(mail.mailTime),
			hrefName, index, port);
}

std::string GenerateTableRow(std::string mailAddr, std::string subject,
		std::string mainContent, std::string timeElapsed,
		std::string hrefName, int index, int port) {

	/* Given a mail, show it as a table row */
	std::string tableRow =
	"					            <tr>\n"
	"					                <td>\n"
	"					                    <div aria-checked='false' aria-disabled='false' class='icheckbox_flat-blue' style='position: relative;'>\n"
	"					                        <input style='position: absolute; opacity: 0;' type='checkbox'>\n"
	"					                            <ins class='iCheck-helper' style='position: absolute; top: 0%; left: 0%; display: block; width: 100%; height: 100%; margin: 0px; padding: 0px; border: 0px; opacity: 0; background: rgb(255, 255, 255);'>\n"
	"					                            </ins>\n"
	"					                        </input>\n"
	"					                    </div>\n"
	"					                </td>\n"
	"					                <td class='mailbox-star'>\n"
	"					                    <a href='#'>\n"
	"					                        <i class='fa fa-star-o text-yellow'>\n"
	"					                        </i>\n"
	"					                    </a>\n"
	"					                </td>\n"
	"					                <td class='mailbox-name'>\n"
	"					                    <a href='http://localhost:" + std::to_string(port) + "/read.html&";
	tableRow += hrefName + std::to_string(index) + "'>\n";
	tableRow += mailAddr;
	tableRow +=
	"					                    </a>\n"
	"					                </td>\n"
	"					                <td class='mailbox-subject'>\n"
	"					                    <b>\n";
	tableRow += subject.substr(0, INBOX_TABLE_ROW_LIMIT);
	tableRow +=
	"					                    </b>\n";
	if (subject.size() < INBOX_TABLE_ROW_LIMIT) {
		tableRow += " - ";
		tableRow += mainContent.substr(0, INBOX_TABLE_ROW_LIMIT - subject.size());
	}

	if (mainContent.size() >= INBOX_TABLE_ROW_LIMIT - subject.size()) {
		tableRow += "...\n";
	}
	tableRow +=
	"					                </td>\n"
	"					                <td class='mailbox-attachment'>\n"
	"					                    <i class='fa fa-paperclip'>\n"
	"					                    </i>\n"
	"					                </td>\n"
	"					                <td class='mailbox-date'>\n";
	tableRow += timeElapsed;
	tableRow +=
	"					                </td>\n"
	"					            </tr>\n";

	return tableRow;
}

inline std::string GetPluralSuffix(int x) {

	/* Give words plural form */
	if (x > 1) {
		return "s";
	} else {
		return "";
	}
}

std::string GetElapsedTimeStr(std::string dateTime) {

	/* Calculate elapsed time */
	long int timeDiff = GetTimeDiffInSecond(dateTime);
	if (verbose) std::cout << "--------------[dateTime]--------------"
				<< dateTime << std::endl;
	if (verbose) std::cout << "--------------[timeDiff]--------------"
			<< timeDiff << std::endl;
	std::string result;

	// Show this time in different unit based on its value
	if (timeDiff <= 60) {
		result = std::to_string(timeDiff) + " second" + GetPluralSuffix(timeDiff);
	} else if (timeDiff <= 3600) {
		timeDiff /= 60;
		result = std::to_string(timeDiff) + " minute" + GetPluralSuffix(timeDiff);
	} else if (timeDiff <= 86400) {
		timeDiff /= 3600;
		result = std::to_string(timeDiff) + " hour" + GetPluralSuffix(timeDiff);
	} else if (timeDiff <= 604800) {
		timeDiff /= 86400;
		result = std::to_string(timeDiff) + " day" + GetPluralSuffix(timeDiff);
	} else if (timeDiff <= 2592000) {
		timeDiff /= 604800;
		result = std::to_string(timeDiff) + " week" + GetPluralSuffix(timeDiff);
	} else if (timeDiff <= 3153600) {
		timeDiff /= 259200;
		result = std::to_string(timeDiff) + " month" + GetPluralSuffix(timeDiff);
	} else {
		timeDiff /= 3153600;
		result = std::to_string(timeDiff) + " year" + GetPluralSuffix(timeDiff);
	}

	result += " ago";

	return result;
}

long int GetTimeDiffInSecond(std::string dateTime) {

	/* Return a time difference */
	time_t t = Str2Time_t(dateTime);
	time_t t_curr = time(NULL);
	long int timeDiffInSecond = (long)t_curr - (long)t - 3600; // winter time
	if (verbose) std::cout << "--------------[t_curr]--------------"
				<< t_curr << std::endl;
	if (verbose) std::cout << "--------------[t]--------------"
				<< t << std::endl;
	return timeDiffInSecond;
}

time_t Str2Time_t(std::string dateTime) {

	/* Parse a string into time_t */
	struct tm tm;
	time_t t;
	if (strptime(dateTime.c_str(), "%a %b %d %H:%M:%S %Y", &tm) == NULL) {
		std::cout << "Fail to parse datetime." << std::endl;
	}
	t = mktime(&tm);
	if (t == -1) {
		std::cout << "Fail to parse time." << std::endl;
	}

	return t;
}

inline void WriteFile(std::string fileName, std::string content) {

	/* Write a string to a file */
	std::ofstream myfile;
	myfile.open(fileName);
	myfile << content;
	myfile.close();
}

inline void OpenResponsePage(std::string responsePage) {

	/* Open a web page in firefox */
	WriteFile("testPage.html", responsePage);

	system("clear");
	system("firefox testPage.html &");

}

inline std::string GetCurrentTime() {

	/* Open a web page in firefox */
	time_t curTime = time(NULL);
	std::string t = asctime(localtime(&curTime));
	return t;
}

void sigint(int a)
{
	/* Shut down teh server if ctrl + c is pressed */
	v_shutdown = 1;
	printf("\n");
	char response[MAX_MSG_LEN] = "-ERR Server shutting down\r\n";
	for (auto socket : socket_vector) {
		send(socket, response, strlen(response), 0);
		if(verbose) printf("Socket %d closed.\n", socket);
		close(socket);
	}
	if(verbose) printf("Server terminated.\n");

	t_mutex.lock();
	// Number of active thread num --.
	active_thread_num--;

	// This thread is no longer occupied.
	thread_occupied[close_thread_id] = false;

	// This thread should be joined.
	thread_finished[close_thread_id] = true;
	t_mutex.unlock();

	exit(1);
}

inline std::string SockAddr2Str(struct sockaddr_in servaddr) {
	/* Convert address to string */
	std::string addr = inet_ntoa(servaddr.sin_addr);
	std::string port = std::to_string(ntohs(servaddr.sin_port));
	return addr + ":" + port;
}

inline struct sockaddr_in Str2SockAddr(std::string servaddrStr) {

	/* Convert string to address */
	struct sockaddr_in serverSocket;

	int fDPos = servaddrStr.find(":");
	int port = std::stoi(servaddrStr.substr(fDPos + 1, servaddrStr.size()));
	servaddrStr = servaddrStr.substr(0, fDPos);

	serverSocket.sin_family = AF_INET;
	serverSocket.sin_port = htons(port);
	inet_pton(AF_INET, servaddrStr.c_str(), &(serverSocket.sin_addr));

	return serverSocket;
}

inline bool StringMatch(char* keyword, const char* pattern) {

	/* Check if strings are matched */
	int len = strlen(pattern);
	for (int i = 0; i < len; i++) {
		// Case-insensitive comparison.
		if (toupper(keyword[i]) != pattern[i]) {
			return false;
		}
	}
	return true;
}

std::vector<std::string> StrSplit
(std::string strToSplit, const char delimiter) {

	/* Split string by a specified delimiter, save the result into a vector */
	std::vector<std::string> splittedStrings;
	if (strToSplit.find(delimiter) < strToSplit.size() &&
			strToSplit.find(delimiter) >= 0) {
	    std::stringstream ss(strToSplit);
	    std::string item;

	    while (std::getline(ss, item, delimiter))
	    {
	       splittedStrings.push_back(item);
	    }
	} else {
		splittedStrings.push_back(strToSplit);
	}

    return splittedStrings;
}

bool closeSocketAfterwards = true;

std::string GetServerResponse
(struct sockaddr_in serverAddr, std::string request, int thread_socket) {

	/* Send message to a server and get its response */
	int newSocket;

    if ((newSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Cannot establish connection with key-value store");
        close(thread_socket);
        if(verbose) printf("Socket %d closed.\n", thread_socket);
        exit(EXIT_FAILURE);
    }
    struct timeval tv;
    tv.tv_sec = 3;  /* 3 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors
    setsockopt(newSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

    if (connect(newSocket,
    		(struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
    	perror("Cannot connect to key-value store");
        close(thread_socket);
        if(verbose) printf("Socket %d closed.\n", thread_socket);
		exit(EXIT_FAILURE);
    }

    if (verbose) std::cout << "[newSocket]" << newSocket << std::endl;
    if (verbose) std::cout << "[request]" << request << std::endl;
    std::string response = GetServerResponse(newSocket, request);

	if (closeSocketAfterwards) {
		close(newSocket);
	}

	return response;
}

void clean(char *var) {

	/* Clean a char array */
    int i = 0;
    while (var[i] != '\0' && i < MAX_MSG_LEN - 1) {
        var[i] = '\0';
        ++i;
    }
}

std::string GetServerResponse(int serverSocket, std::string request) {

	/* Put message into a socket and get its response */
	bool svOK = false;
	std::string response;

	// Send message
	SendMessage(serverSocket, request);
	char buffer[MAX_MSG_LEN];
	clean(buffer);

	svOK = true;

	if (read(serverSocket, buffer, MAX_MSG_LEN) == -1) {
		fprintf(stderr, "Fail to read server response.\n");
		svOK = false;
		response = "Timeout";
	} else {
		response = buffer;
	}

	if(verbose) std::cout << response << std::endl;
	return response;
}

void SendMessage(int socket, std::string message) {

	/* Put message into a socket */
	send(socket, message.c_str(), message.size(), 0);
	if(verbose) printf("[%d] S: %s", socket, message.c_str());
}

#endif /* MAIL_HH_ */
