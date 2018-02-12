#ifndef _STORAGE_HH
#define _STORAGE_HH

#include <string>
#include <vector>
using namespace std;

static const string header = "<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<title>Penn Cloud Drive</title>\n"
"<link rel=\"stylesheet\" type=\"text/css\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">\n"
"<script src=\"https://use.fontawesome.com/f6ba3790a7.js\"></script>\n"
"</head>\n\n"
"<body>\n";
// "<h1>Penn Cloud Drive</h1>\n";
static const string footer = "</body>\n"
"</html>\n";
static const string navbar_header = "<nav class=\"navbar navbar-default\">\n"
"<div class=\"container-fluid\">\n"
"<div class=\"navbar-header\">\n"
"<a class=\"navbar-brand\" href=\"/\">Penn Cloud Drive</a>\n"
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
"<th>Name</th>\n"
"<th>Manage</th>\n"
"</tr>\n";
static const string table_footer = "</table>\n";
static const string go_back = "<a href=\"/\" class=\"btn btn-primary\">Go Back</a>\n";

static const string ok_200 = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
static const string ok_200_file = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n";
static const string error_301 = "HTTP/1.1 301 Moved Permanently\r\nLocation: ";
static const string error_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
static const string error_500 = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n";
static const string error_503 = "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/html\r\n\r\n";

static const string view_route = "/view";
static const string upload_route = "/upload";
static const string download_route = "/download";
static const string folder_route = "/folder";
static const string rename_form_route = "/rename_form";
static const string rename_route = "/rename";
static const string move_form_route = "/move_form";
static const string move_route = "/move";
static const string delete_route = "/delete";

#endif
