#ifndef _ADMIN_HH
#define _ADMIN_HH

#include <string>
#include <vector>
using namespace std;

class Node {
public:
	int id;
	string type;
	string address;
	string status;
	bool primary;

	Node(int id, string type, string address, string status, bool primary):
		id(id), type(type), address(address), status(status), primary(primary) {}
};

class Value {
public:
	string row;
	string col;
	string val;

	Value(string r, string c, string v) : row(r), col(c), val(v) {}
};

static const string header = "<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<title>Penn Cloud Admin Console</title>\n"
"<link rel=\"stylesheet\" type=\"text/css\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">\n"
"<script src=\"https://use.fontawesome.com/f6ba3790a7.js\"></script>\n"
"</head>\n"
"<body>\n";
// "<h1>Penn Cloud Admin Console</h1>\n";
static const string footer = "</body>\n"
"</html>\n";
static const string navbar_header = "<nav class=\"navbar navbar-default\">\n"
"<div class=\"container-fluid\">\n"
"<div class=\"navbar-header\">\n"
"<a class=\"navbar-brand\" href=\"/\">Penn Cloud Admin Console</a>\n"
"</div>\n"
"<div class=\"collapse navbar-collapse\">\n"
"<ul class=\"nav navbar-nav navbar-right\">"
"<li><a href=\"#\"><strong>";
static const string navbar_footer = "</strong></a></li>\n"
"</ul>\n"
"</div>\n"
"</div>\n"
"</nav>\n";
static const string table_header = "<table class=\"table table-hover\">\n"
"<tr>\n"
"<th>ID</th>\n"
"<th>Type</th>\n"
"<th>Address</th>\n"
"<th>Status</th>\n"
"<th>Primary</th>\n"
"<th>View Data</th>\n"
"</tr>\n";
static const string table_footer = "</table>\n";
static const string data_header = "<table class=\"table table-hover\">\n"
"<tr>\n"
"<th>Row Key</th>\n"
"<th>Column Key</th>\n"
"<th>Value</th>\n"
"</tr>\n";
static const string data_footer = "</table>\n";
static const string go_back = "<a href=\"/\" class=\"btn btn-primary\">Go Back</a>\n";

static const string ok_200 = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
static const string error_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
static const string error_500 = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n";
static const string error_503 = "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/html\r\n\r\n";

#endif
