#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <pthread.h>
#define random(x) (rand()%x)

using namespace std;

/* Global variables */
const string COMMA = ",";
const string COLON = ":";
const char *ARE_YOU_OK = "/CHECK\r\n";
const char *NEW_PRIME = "/PRIME\r\n";
const char *ALL_DOWN = " back-end server down.\r\n";
const char *NO_ONE = " back-end server.\r\n";
const char *UNKNOWN = "-ERR Unknown command.\r\n";
int RESPONSE_TIMEOUT = 2; // time limit for back-end server response
int CONNECTION_TIMEOUT = 2; // time limit for back-end server connection
int TRY_TIME = 2; // trying time for each node
int mem_limit = 30; // limit for the thread memory

const string STORAGE1 = "STORAGE1";
const string STORAGE2 = "STORAGE2";
const string STORAGE3 = "STORAGE3";
const string MAIL1 = "MAIL1";
const string MAIL2 = "MAIL2";
const string USER = "USER";

//**********************************
void cleanLogHelper(string serverTablet)
{
    string rmRecover = "log/recoverLog" + serverTablet + ".txt";
    string rmKeyValue = "log/keyValuesLog" + serverTablet + ".txt";
    ofstream tempRecover;
    ofstream tempKeyValue;
    remove(rmRecover.c_str());
    remove(rmKeyValue.c_str());
    tempRecover.open(rmRecover, ios::app);
    tempKeyValue.open(rmKeyValue, ios::app);
    tempKeyValue.close();
    tempRecover.close();
}

void cleanLog()
{
    cleanLogHelper(STORAGE1);
    cleanLogHelper(STORAGE2);
    cleanLogHelper(STORAGE3);
    cleanLogHelper(MAIL1);
    cleanLogHelper(MAIL2);
    cleanLogHelper(USER);
}
//*********************************

pthread_mutex_t STORAGE_P_LOCK1, STORAGE_P_LOCK2, STORAGE_P_LOCK3;
pthread_mutex_t MAIL_P_LOCK1, MAIL_P_LOCK2, USER_P_LOCK;
vector<pthread_t> THREADS;
vector<unsigned int> SOCKETS;
vector<unsigned int> FINISHED;
vector<sockaddr_in> STORAGE_SERVERS1, STORAGE_SERVERS2, STORAGE_SERVERS3;
vector<int> STORAGE_OK1, STORAGE_OK2, STORAGE_OK3;
vector<sockaddr_in> MAIL_SERVERS1, MAIL_SERVERS2, USER_SERVERS;
vector<int> MAIL_OK1, MAIL_OK2, USER_OK;
unsigned int listen_fd;
sockaddr_in master_addr;
string CF_NAME;
int STORAGE_P_IDX1, STORAGE_P_IDX2, STORAGE_P_IDX3;
int MAIL_P_IDX1, MAIL_P_IDX2, USER_P_IDX;
bool DEBUG;
bool RUNNING;
pthread_t recycle;

/* Function declarations */
void sig_handler(int arg);
void *recycle_t(void *p);
void set_nonblocking(unsigned int fd);
string debug_str();
void admin_check(unsigned int fd);
string get_status(int status);
sockaddr_in to_sockaddr(string addr);
unsigned int get_socket();
void do_read(unsigned int fd, char *buffer, string mid);
void do_read_time(unsigned int fd, char *buffer, string mid);
bool is_server(char *buffer, int &index, int &kind);
void *fd_thread(void *p);
void front_handler(unsigned int fd);
void back_handler(unsigned int fd, int index, int kind);
void back_assign(int self, int index, int kind, int size, string name, vector<sockaddr_in> addrs, vector<int> &oks);

/****************************************************************************************************************************************

This is the master node that deals with server assignment and primary replica fault tolerance.

For front-end servers:
* The master node receives a command "/MAIL [username]\r\n" or "/STOR [username]\r\n" or "/USER [username]\r\n" from the front-end server.
After receiving the command the master node will send a "/CHECK\r\n" command to a random back-end server to check if it is working.
The back-end server should response with a "+GOOD\r\n" to confirm that it is working, or the master will check by token ring sequence.
After checked with back-end server, the master node will response with "+OKIP [IP:port number]\r\n" to the front-end server.
* A command "/ADMIN\r\n" from an admin console will response with all the self-check result of the master node.
The checked message contains time, number of nodes of each tablet, status of each node, primary replica index and other things.

For back-end servers:
* The back-end server should also send a heart beat response "#S:[IP:port number] +GOOD\r\n" for usual checking.
* A command "#S:[IP:port number] /CURP\r\n" from an back-end server will ask the master about the current primary replica.
The master will respond with "+OKCPR [index]\r\n" where the index is the back-end server's tablet's current primary replica.
Every time when the back-end server starts, it should ask the master node for its tablet's primary replica.
* A command "#S:[IP:port number] /NEWP [index]\r\n" from an back-end server will let the master node to assign a new primary replica.
The index here is the current prime that the back-end server knows. This is used to avoid repeated assignment.
The master node will send a "/PRIME\r\n" to a back-end server to check if it could become a new primary replica, by token ring sequence.
If the back-end server is able to become new primary replica, it will response with a "+ROGER\r\n" to confirm.
Then the master will send all back-end servers in their tablet with "+OKNPR [index]\r\n" where the index is the new primary replica.

** To run the master node, use command like "./masterNode -v backend.txt" where -v is for debug messages and serverlist.txt is
the "IP:port number" configuration file for master node and back-end servers for different tablets.

*****************************************************************************************************************************************/

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stderr, "*** Author: Gongyao Chen (gongyaoc)\n");
        exit(1);
    }

    //**********************************
    cleanLog();
    //**********************************

    /* Handling shutdown signal */
    signal(SIGINT, sig_handler);

    /* Parsing command line arguments */
    int ch = 0;
    while ((ch = getopt(argc, argv, "v")) != -1)
    {
        switch (ch)
        {
        case 'v':
            DEBUG = true;
            break;
        case '?':
            fprintf(stderr, "Master: Error: Invalid choose: %c\n", (char) optopt);
            exit(1);
        default:
            fprintf(stderr,
                    "Master: Error: Please input [-v] [configuration file]\n");
            exit(1);
        }
    }
    if (optind == argc)
    {
        fprintf(stderr, "Master: Error: Please input [configuration file] [index]\n");
        exit(1);
    }
    CF_NAME = argv[optind];

    struct sockaddr_in server_addr, client_addr; // Structures to represent the server and client

    /* Parse configuration file */
    ifstream cong_f;
    cong_f.open(CF_NAME);
    ch = -1;
    string ln;
    while (getline(cong_f, ln))
    {
        if(ln == "MASTER")
        {
            ch = 0;
            continue;
        }
        else if(ln == "STORAGE1")
        {
            ch = 11;
            continue;
        }
        else if(ln == "STORAGE2")
        {
            ch = 12;
            continue;
        }
        else if(ln == "STORAGE3")
        {
            ch = 13;
            continue;
        }
        else if(ln == "MAIL1")
        {
            ch = 21;
            continue;
        }
        else if(ln == "MAIL2")
        {
            ch = 22;
            continue;
        }
        else if(ln == "USER")
        {
            ch = 3;
            continue;
        }
        int comma_idx = ln.find(COMMA);
        string serv_ad = ln.substr(0, comma_idx);
        string binding = ln.substr(comma_idx + 1, ln.length());
        if (ch == 0)
        {
            if (binding != "")
            {
                server_addr = to_sockaddr(binding);
            }
            else
            {
                server_addr = to_sockaddr(serv_ad);
            }
        }
        else if(ch == 11)
        {
            STORAGE_SERVERS1.push_back(to_sockaddr(serv_ad));
            STORAGE_OK1.push_back(-1);
        }
        else if(ch == 12)
        {
            STORAGE_SERVERS2.push_back(to_sockaddr(serv_ad));
            STORAGE_OK2.push_back(-1);
        }
        else if(ch == 13)
        {
            STORAGE_SERVERS3.push_back(to_sockaddr(serv_ad));
            STORAGE_OK3.push_back(-1);
        }
        else if(ch == 21)
        {
            MAIL_SERVERS1.push_back(to_sockaddr(serv_ad));
            MAIL_OK1.push_back(-1);
        }
        else if(ch == 22)
        {
            MAIL_SERVERS2.push_back(to_sockaddr(serv_ad));
            MAIL_OK2.push_back(-1);
        }
        else if(ch == 3)
        {
            USER_SERVERS.push_back(to_sockaddr(serv_ad));
            USER_OK.push_back(-1);
        }
    }

    /* Create a new socket */
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Master: Socket open error.\n");
        exit(1);
    }
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse,
                   sizeof(int)) == -1)
    {
        fprintf(stderr, "Master: Socket set error.\n");
        exit(1);
    }

    /* Configure the server */
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    /* Use the socket and associate it with the port number */
    if (bind(listen_fd, (const struct sockaddr *) &server_addr,
             sizeof(struct sockaddr)) == -1)
    {
        fprintf(stderr, "Master: Unable to bind.\n");
        exit(1);
    }

    /* Start to listen client connections */
    if (listen(listen_fd, 100) == -1)
    {
        fprintf(stderr, "Master: Unable to listen.\n");
        exit(1);
    }
    master_addr = server_addr;
    RUNNING = true;
    if (DEBUG)
    {
        printf("Master node configured to listen on IP: %s, port#: %d\n",
               inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
    }
    fflush(stdout);
    pthread_mutex_init(&STORAGE_P_LOCK1, NULL);
    pthread_mutex_init(&STORAGE_P_LOCK2, NULL);
    pthread_mutex_init(&STORAGE_P_LOCK3, NULL);
    pthread_mutex_init(&MAIL_P_LOCK1, NULL);
    pthread_mutex_init(&MAIL_P_LOCK2, NULL);
    pthread_mutex_init(&USER_P_LOCK, NULL);
    pthread_create(&recycle, NULL, &recycle_t, NULL);

    while(RUNNING)
    {
        socklen_t client_len = sizeof(client_addr);
        unsigned int comm_fd = accept(listen_fd,
                                      (struct sockaddr *) &client_addr, &client_len);
        if (!RUNNING || comm_fd == -1)
        {
            break;
        }
        //set_nonblocking(comm_fd); // set fd to non-blocking
        /* Assign the incoming fd to a thread */
        SOCKETS.push_back(comm_fd);
        pthread_t thread;
        pthread_create(&thread, NULL, &fd_thread, &comm_fd);
        THREADS.push_back(thread);
    }

    if (DEBUG)
    {
        printf("Master node successfully shut down.\n");
    }
    return 0;
}

/* Thread function for a incoming socket */
void *fd_thread(void *p)
{
    unsigned int comm_fd = *(int *) p;
    char head[257] = { }; // read the head of a message for classification
    char *tail;
    while (RUNNING && (tail = strstr(head, "\r\n")) == NULL)
    {
        int len = strlen(head);
        if(len > 160)
        {
            break;
        }
        int recv_len = recv(comm_fd, head, 256, MSG_PEEK);
        if(recv_len == -1)
        {
            continue;
        }
    }

    /* Income is back-end server */
    int index = 0, kind = 0; // kind 1x for storage, 2x for mail and 3 for user
    if(is_server(head, index, kind) && RUNNING)
    {
        if (DEBUG)
        {
            string name = (kind == 11) ? "storage p1" : (kind == 12) ? "storage p2" : (kind == 13) ? "storage p3" :
                          (kind == 21) ? "mail p1" : (kind == 22) ? "mail p1" : "user";
            fprintf(stderr, "%s [%d] New back-end %s server #%d connection.\n",
                    debug_str().c_str(), comm_fd, name.c_str(), index);
        }
        back_handler(comm_fd, index, kind);
    }

    /* Income is front-end server */
    else if(RUNNING)
    {
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] New front end server connection.\n",
                    debug_str().c_str(), comm_fd);
        }
        front_handler(comm_fd);
    }

    FINISHED.push_back(comm_fd);
    return NULL;
}

/* Handler function for a back-end server */
void back_handler(unsigned int fd, int index, int kind)
{
    vector<sockaddr_in> addrs = (kind == 11) ? STORAGE_SERVERS1 : (kind == 12) ? STORAGE_SERVERS2 : (kind == 13) ? STORAGE_SERVERS3 :
                                (kind == 21) ? MAIL_SERVERS1 : (kind == 22) ? MAIL_SERVERS2 : USER_SERVERS;
    vector<int> &oks = (kind == 11) ? STORAGE_OK1 : (kind == 12) ? STORAGE_OK2 : (kind == 13) ? STORAGE_OK3 :
                       (kind == 21) ? MAIL_OK1 : (kind == 22) ? MAIL_OK2 : USER_OK;
    int size = (kind == 11) ? STORAGE_SERVERS1.size() : (kind == 12) ? STORAGE_SERVERS2.size() : (kind == 13) ? STORAGE_SERVERS3.size() :
               (kind == 21) ? MAIL_SERVERS1.size() : (kind == 22) ? MAIL_SERVERS2.size() : USER_SERVERS.size();
    int range = (kind == 11) ? STORAGE_SERVERS1.size() : (kind == 12) ? STORAGE_SERVERS2.size() + STORAGE_SERVERS1.size() :
                (kind == 13) ? STORAGE_SERVERS3.size() + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() :
                (kind == 21) ? MAIL_SERVERS1.size() + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() :
                (kind == 22) ? MAIL_SERVERS2.size() + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() :
                USER_SERVERS.size() + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() + MAIL_SERVERS2.size();
    int cur_p_idx = (kind == 11) ? STORAGE_P_IDX1 : (kind == 12) ? STORAGE_P_IDX2 + STORAGE_SERVERS1.size() :
                    (kind == 13) ? STORAGE_P_IDX3 + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() :
                    (kind == 21) ? MAIL_P_IDX1 + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() :
                    (kind == 22) ? MAIL_P_IDX2 + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() :
                    USER_P_IDX + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() + MAIL_SERVERS2.size();
    string name = (kind == 11) ? "storage p1" : (kind == 12) ? "storage p2" : (kind == 13) ? "storage p3" : (kind == 21) ? "mail p1" :
                  (kind == 22) ? "mail p2" : "user";

    char buffer[1025] = { };
    string mid = "back-end " + name + " server #" + to_string(index);
    do_read(fd, buffer, mid);

    char *buff = buffer;
    while(buff[0] != ' ')
    {
        buff++;
    }
    buff++;
    char command[7] = { }; // get back-end server's response
    for (int i = 0; i < 5; i++)
    {
        command[i] = buff[i];
    }
    if (strcmp(command, "+GOOD") == 0 && strlen(buff) == 7) // decide if a back-end server is working by heart beat
    {
        oks[index] = 1; // status 1 means working
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] Back-end %s server #%d heart beat.\n",
                    debug_str().c_str(), fd, name.c_str(), index);
        }
        close(fd);
        return;
    }
    else if(strcmp(command, "/CURP") == 0 && strlen(buff) == 7)
    {
        string response = "+OKCPR " + to_string(cur_p_idx) + "\r\n";
        const char *res = response.c_str();
        write(fd, res, strlen(res));
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] Respond to back-end %s server #%d for current primary: %s\n",
                    debug_str().c_str(), fd, name.c_str(), index, res);
        }
        close(fd);
        return;
    }
    else if(strcmp(command, "/NEWP") == 0 && strlen(buff) >= 9)
    {
        int i = 6, idx = 0;
        while(buff[i] != '\r' && ((buff[i] - '0' <= 9) && (buff[i] - '0' >= 0)))
        {
            idx = idx * 10 + (buff[i] - '0');
            i++;
        }
        if(RUNNING && idx < range) // assign the new primary replica
        {
            if (DEBUG)
            {
                fprintf(stderr, "%s [%d] Back-end %s server #%d request assigning new primary.\n",
                        debug_str().c_str(), fd, name.c_str(), index);
            }
            back_assign(index, idx, kind, size, name, addrs, oks);
            close(fd);
            return;
        }
        else if(RUNNING)
        {
            if (DEBUG)
            {
                fprintf(stderr, "%s [%d] Back-end %s server #%d has primary index problem.\n",
                        debug_str().c_str(), fd, name.c_str(), index);
            }
        }
    }

    if(RUNNING)
    {
        oks[index] = -3; // status -3 means unknown error
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] Back-end %s server #%d has unknown problem.\n",
                    debug_str().c_str(), fd, name.c_str(), index);
        }
        close(fd);
    }
}

/* Function for assigning a new primary replica */
void back_assign(int self, int index, int kind, int size, string name, vector<sockaddr_in> addrs, vector<int> &oks)
{
    int &try_p_idx = (kind == 11) ? STORAGE_P_IDX1 : (kind == 12) ? STORAGE_P_IDX2 : (kind == 13) ? STORAGE_P_IDX3 :
                     (kind == 21) ? MAIL_P_IDX1 : (kind == 22) ? MAIL_P_IDX2 : USER_P_IDX;
    pthread_mutex_t &lock = (kind == 11) ? STORAGE_P_LOCK1 : (kind == 12) ? STORAGE_P_LOCK2 : (kind == 13) ? STORAGE_P_LOCK3 :
                            (kind == 21) ? MAIL_P_LOCK1 : (kind == 22) ? MAIL_P_LOCK2 : USER_P_LOCK;
    bool all = true;
    bool checked = true;

    pthread_mutex_lock(&lock); // always only let one thread of a tablet to assign
    int cur_p_idx = (kind == 11) ? STORAGE_P_IDX1 : (kind == 12) ? STORAGE_P_IDX2 + STORAGE_SERVERS1.size() :
                    (kind == 13) ? STORAGE_P_IDX3 + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() :
                    (kind == 21) ? MAIL_P_IDX1 + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() :
                    (kind == 22) ? MAIL_P_IDX2 + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() :
                    USER_P_IDX + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() + MAIL_SERVERS2.size();
    all = (index == cur_p_idx) ? true : false; // judge if the assignment is completed
    if(all)  // only first request needs reassignment
    {
        checked = false;
        int idx = (try_p_idx + 1) % size;
        int count = 0;
        bool connected = false;
        while(RUNNING && count < (size * TRY_TIME))
        {
            if(idx == try_p_idx)  // new primary should not be the same as the current one
            {
                continue;
            }
            if (DEBUG)
            {
                fprintf(stderr, "%s Assigning primary to back-end %s server #%d.\n",
                        debug_str().c_str(), name.c_str(), idx);
            }
            unsigned int out = get_socket();
            time_t now = time(NULL), pre = time(NULL);
            while(RUNNING && (int)(now - pre) < CONNECTION_TIMEOUT)
            {
                now = time(NULL);
                int ret = connect(out, (struct sockaddr *) & (addrs[idx]), sizeof(addrs[idx])); // establish a connection with a back-end server
                if(ret != -1)
                {
                    write(out, NEW_PRIME, strlen(NEW_PRIME)); // try assigning with a back-end server
                    connected = true;
                    break;
                }
            }
            if(!connected)
            {
                oks[idx] = -1; // status -1 means connection timeout
                if (DEBUG)
                {
                    fprintf(stderr, "%s [%d] Back-end %s server #%d connection timeout.\n",
                            debug_str().c_str(), out, name.c_str(), idx);
                }
            }

            char resp[1025] = { };
            oks[idx] = -2; // set status -2 as response timeout
            while(RUNNING && connected && ((int)(now - pre) < RESPONSE_TIMEOUT)) // wait for response
            {
                now = time(NULL);
                string mid = "back-end " + name + " server #" + to_string(idx);
                do_read_time(out, resp, mid);
                char command[7] = { }; // get back-end server's response
                for (int i = 0; i < 6; i++)
                {
                    command[i] = resp[i];
                }
                if (strcmp(command, "+ROGER") == 0  && strlen(resp) == 8) // decide if a back-end server is working
                {
                    checked = true;
                    break;
                }
                else
                {
                    oks[idx] = -3; // status -3 means unknown error
                }
            }
            close(out);

            if(checked)
            {
                try_p_idx = idx;
                break;
            }
            else if (DEBUG && oks[idx] == -2)
            {
                fprintf(stderr, "%s [%d] Back-end %s server #%d response timeout.\n",
                        debug_str().c_str(), out, name.c_str(), idx);
            }
            else if(DEBUG && oks[idx] == -3)
            {
                fprintf(stderr, "%s [%d] Back-end %s server #%d has unknown problem.\n",
                        debug_str().c_str(), out, name.c_str(), idx);
            }
            idx = (idx + 1) % size;
            count++;
        }
    }
    pthread_mutex_unlock(&lock);

    int new_p_idx = (kind == 11) ? try_p_idx : (kind == 12) ? try_p_idx + STORAGE_SERVERS1.size() :
                    (kind == 13) ? try_p_idx + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() :
                    (kind == 21) ? try_p_idx + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() :
                    (kind == 22) ? try_p_idx + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() :
                    try_p_idx + STORAGE_SERVERS1.size() + STORAGE_SERVERS2.size() + STORAGE_SERVERS3.size() + MAIL_SERVERS1.size() + MAIL_SERVERS2.size();
    string response = "+OKNPR " + to_string(new_p_idx) + "\r\n"; // make reulst for new primary
    const char *res = response.c_str();
    time_t nnow = time(NULL), npre = time(NULL);
    bool first = true;
    for(int i = 0; i < size; i++)
    {
        if(first)
        {
            i = try_p_idx;
        }
        else if(i == try_p_idx)
        {
            continue;
        }
        if(all || i == self)
        {
            unsigned int nout = get_socket();
            while((int)(nnow - npre) < CONNECTION_TIMEOUT)
            {
                nnow = time(NULL);
                int nret = connect(nout, (struct sockaddr *) & (addrs[i]), sizeof(addrs[i])); // establish a connection with a back-end server
                if(nret != -1)
                {
                    write(nout, res, strlen(res)); // try assigning with a back-end server
                    if (DEBUG)
                    {
                        fprintf(stderr, "%s [%d] Respond to back-end %s server #%d for new primary: %s\n",
                                debug_str().c_str(), nout, name.c_str(), i, res);
                    }
                    break;
                }
            }
            close(nout);
        }
        if(first)
        {
            i = -1;
            first = false;
        }
    }
}

/* Function for handling a front-end server */
void front_handler(unsigned int fd)
{
    srand((int)time(0));
    int ch = 0, size = 0, index = 0;
    string monitor = "";
    string name = "";

    char buffer[1025] = { };
    string mid = "front-end server";
    do_read(fd, buffer, mid);

    char command[7] = { }; // get command from front-end server
    for (int i = 0; i < 5; i++)
    {
        command[i] = buffer[i];
    }
    if(strcmp(command, "/STOR") == 0)
    {
        if(((buffer[6] - '0' >= 0) && (buffer[6] - '0' <= 9)) || (buffer[6] >= 'A' && buffer[6] <= 'F') ||
                (buffer[6] >= 'a' && buffer[6] <= 'f'))
        {
            ch = 11;
            size = STORAGE_SERVERS1.size();
        }
        else if((buffer[6] >= 'G' && buffer[6] <= 'P') || (buffer[6] >= 'g' && buffer[6] <= 'p'))
        {
            ch = 12;
            size = STORAGE_SERVERS2.size();
        }
        else if((buffer[6] >= 'Q' && buffer[6] <= 'Z') || (buffer[6] >= 'q' && buffer[6] <= 'z'))
        {
            ch = 13;
            size = STORAGE_SERVERS3.size();
        }
    }
    else if(strcmp(command, "/MAIL") == 0)
    {
        if(((buffer[6] - '0' >= 0) && (buffer[6] - '0' <= 9)) || (buffer[6] >= 'A' && buffer[6] <= 'J') ||
                (buffer[6] >= 'a' && buffer[6] <= 'j'))
        {
            ch = 21;
            size = MAIL_SERVERS1.size();
        }
        if((buffer[6] >= 'K' && buffer[6] <= 'Z') || (buffer[6] >= 'k' && buffer[6] <= 'z'))
        {
            ch = 22;
            size = MAIL_SERVERS2.size();
        }
    }
    else if(strcmp(command, "/USER") == 0)
    {
        ch = 3;
        size = USER_SERVERS.size();
    }
    else
    {
        command[5] = buffer[5];
        if(strcmp(command, "/ADMIN") == 0)
        {
            admin_check(fd);
            return;
        }
    }

    name = (ch == 11) ? "storage p1" : (ch == 12) ? "storage p2" : (ch == 13) ? "storage p3" : (ch == 21) ? "mail p1" :
           (ch == 22) ? "mail p2" : "user";
    if(ch == 0)
    {
        monitor = UNKNOWN; // unknown command
    }
    else if(size == 0)
    {
        monitor = "-ERR No " + name + NO_ONE; // no such server
    }
    else // try to assign a back-end server and respond with address
    {
        vector<sockaddr_in> addrs = (ch == 11) ? STORAGE_SERVERS1 : (ch == 12) ? STORAGE_SERVERS2 : (ch == 13) ? STORAGE_SERVERS3 :
                                    (ch == 21) ? MAIL_SERVERS1 : (ch == 22) ? MAIL_SERVERS2 : USER_SERVERS;
        vector<int> &oks = (ch == 11) ? STORAGE_OK1 : (ch == 12) ? STORAGE_OK2 : (ch == 13) ? STORAGE_OK3 :
                           (ch == 21) ? MAIL_OK1 : (ch == 22) ? MAIL_OK2 : USER_OK;
        index = random(size);
        bool checked = true;

        if(oks[index] != 1)  // skip if server has already been checked by others or heart beat
        {
            checked = false;
            int count = 0;
            bool connected = false;
            while(RUNNING && count < (size * TRY_TIME))
            {
                if (DEBUG)
                {
                    fprintf(stderr, "%s Checking back-end %s server #%d.\n",
                            debug_str().c_str(), name.c_str(), index);
                }
                unsigned int out = get_socket();
                time_t now = time(NULL), pre = time(NULL);
                while(RUNNING && (int)(now - pre) < CONNECTION_TIMEOUT)
                {
                    now = time(NULL);
                    int ret = connect(out, (struct sockaddr *) & (addrs[index]), sizeof(addrs[index])); // establish a connection with a back-end server
                    if(ret != -1)
                    {
                        write(out, ARE_YOU_OK, strlen(ARE_YOU_OK)); // try checking with a back-end server
                        connected = true;
                        break;
                    }
                }
                if(!connected)
                {
                    oks[index] = -1; // status -1 means connection timeout
                    if (DEBUG)
                    {
                        fprintf(stderr, "%s [%d] Back-end %s server #%d connection timeout.\n",
                                debug_str().c_str(), out, name.c_str(), index);
                    }
                }

                char resp[1025] = { };
                oks[index] = -2; // set status -2 as response timeout
                while(RUNNING && connected && ((int)(now - pre) < RESPONSE_TIMEOUT)) // wait for response
                {
                    now = time(NULL);
                    mid = "back-end " + name + " server #" + to_string(index);
                    do_read_time(out, resp, mid);
                    char rcommand[6] = { }; // get back-end server's response
                    for (int i = 0; i < 5; i++)
                    {
                        rcommand[i] = resp[i];
                    }
                    if (strcmp(rcommand, "+GOOD") == 0  && strlen(resp) == 7) // decide if a back-end server is working
                    {
                        oks[index] = 1;
                        checked = true;
                        break;
                    }
                    else
                    {
                        oks[index] = -3; // status -3 means unknown error
                    }
                }
                close(out);

                if(checked)
                {
                    break;
                }
                else if (DEBUG && oks[index] == -2)
                {
                    fprintf(stderr, "%s [%d] Back-end %s server #%d response timeout.\n",
                            debug_str().c_str(), out, name.c_str(), index);
                }
                else if(DEBUG && oks[index] == -3)
                {
                    fprintf(stderr, "%s [%d] Back-end %s server #%d has unknown problem.\n",
                            debug_str().c_str(), out, name.c_str(), index);
                }
                index = (index + 1) % size;
                count++;
            }
        }

        if(checked) // assign the checked back-end server
        {
            char ip_name[256] = { };
            sprintf(ip_name, "%s:%d", inet_ntoa(addrs[index].sin_addr),
                    ntohs(addrs[index].sin_port));
            monitor = "+OKIP " + string(ip_name) + "\r\n";
            oks[index] = 0; // reset to unchecked for other use
        }
        else
        {
            monitor = "-ERR All " + name + ALL_DOWN; // all such server down
        }
    }

    const char *res = monitor.c_str();
    if(RUNNING)
    {
        write(fd, res, strlen(res)); // respond result to front-end server
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] Front-end assigned result: %s\n",
                    debug_str().c_str(), fd, res);
        }

        close(fd);
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] Front-end server connection closed.\n",
                    debug_str().c_str(), fd);
        }
    }
}

/* Response to the admin console with self-check result */
void admin_check(unsigned int fd)
{
    string response = "Master node check result:\nLocal time: ";
    response += debug_str() + "\nMaster node: "; // add time stamp
    response += string(inet_ntoa(master_addr.sin_addr)) + ":"
                + to_string(ntohs(master_addr.sin_port)) + "\n"; // add master node address

    response += "Storage1 node count: " + to_string(STORAGE_SERVERS1.size()) + "\n"; // add storage node number
    for(int i = 0; i < STORAGE_SERVERS1.size(); i++) // add each storage node's status
    {
        sockaddr_in server = STORAGE_SERVERS1[i];
        string status = get_status(STORAGE_OK1[i]);
        response += to_string(i) + ": " + string(inet_ntoa(server.sin_addr))
                    + ":" + to_string(ntohs(server.sin_port)) + " Status: " + status + "\n";
    }
    response += "Storage1 primary replica index: " + to_string(STORAGE_P_IDX1) + "\n"; // add storage primary replica index
    response += "Storage2 node count: " + to_string(STORAGE_SERVERS2.size()) + "\n"; // add storage node number
    for(int i = 0; i < STORAGE_SERVERS2.size(); i++) // add each storage node's status
    {
        sockaddr_in server = STORAGE_SERVERS2[i];
        string status = get_status(STORAGE_OK2[i]);
        response += to_string(i) + ": " + string(inet_ntoa(server.sin_addr))
                    + ":" + to_string(ntohs(server.sin_port)) + " Status: " + status + "\n";
    }
    response += "Storage2 primary replica index: " + to_string(STORAGE_P_IDX2) + "\n"; // add storage primary replica index
    response += "Storage3 node count: " + to_string(STORAGE_SERVERS3.size()) + "\n"; // add storage node number
    for(int i = 0; i < STORAGE_SERVERS3.size(); i++) // add each storage node's status
    {
        sockaddr_in server = STORAGE_SERVERS3[i];
        string status = get_status(STORAGE_OK3[i]);
        response += to_string(i) + ": " + string(inet_ntoa(server.sin_addr))
                    + ":" + to_string(ntohs(server.sin_port)) + " Status: " + status + "\n";
    }
    response += "Storage3 primary replica index: " + to_string(STORAGE_P_IDX3) + "\n"; // add storage primary replica index
    response += "Mail1 node count: " + to_string(MAIL_SERVERS1.size()) + "\n"; // add mail node number
    for(int i = 0; i < MAIL_SERVERS1.size(); i++) // add each mail node's status
    {
        sockaddr_in server = MAIL_SERVERS1[i];
        string status = get_status(MAIL_OK1[i]);
        response += to_string(i) + ": " + string(inet_ntoa(server.sin_addr))
                    + ":" + to_string(ntohs(server.sin_port)) + " Status: " + status + "\n";
    }
    response += "Mail1 primary replica index: " + to_string(MAIL_P_IDX1) + "\n"; // add mail primary replica index
    response += "Mail2 node count: " + to_string(MAIL_SERVERS2.size()) + "\n"; // add mail node number
    for(int i = 0; i < MAIL_SERVERS2.size(); i++) // add each mail node's status
    {
        sockaddr_in server = MAIL_SERVERS2[i];
        string status = get_status(MAIL_OK2[i]);
        response += to_string(i) + ": " + string(inet_ntoa(server.sin_addr))
                    + ":" + to_string(ntohs(server.sin_port)) + " Status: " + status + "\n";
    }
    response += "Mail2 primary replica index: " + to_string(MAIL_P_IDX2) + "\n"; // add mail primary replica index
    response += "User node count: " + to_string(USER_SERVERS.size()) + "\n"; // add user node number
    for(int i = 0; i < USER_SERVERS.size(); i++) // add each user node's status
    {
        sockaddr_in server = USER_SERVERS[i];
        string status = get_status(USER_OK[i]);
        response += to_string(i) + ": " + string(inet_ntoa(server.sin_addr))
                    + ":" + to_string(ntohs(server.sin_port)) + " Status: " + status + "\n";
    }
    response += "User primary replica index: " + to_string(USER_P_IDX) + "\n"; // add user primary replica index

    const char *admin_res = response.c_str();
    if(RUNNING)
    {
        write(fd, admin_res, strlen(admin_res)); // respond result to admin console
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] Admin console result: %s\n",
                    debug_str().c_str(), fd, admin_res);
        }

        FINISHED.push_back(fd);
        close(fd);
        if (DEBUG)
        {
            fprintf(stderr, "%s [%d] Admin console connection closed.\n",
                    debug_str().c_str(), fd);
        }
    }
}

/* Return the status of a back-end server */
string get_status(int status)
{
    string res = "What happened?!";
    if(status == 0)
    {
        res = "Normal";
    }
    else if(status == 1)
    {
        res = "Checked";
    }
    else if(status == -1)
    {
        res = "Connection timeout";
    }
    else if(status == -2)
    {
        res = "Response timeout";
    }
    else if(status == -3)
    {
        res = "Unknown problem";
    }
    return res;
}

/* Check if sender is a back-end server */
bool is_server(char *head, int &index, int &kind)
{
    if(head[0] != '#' || head[1] != 'S' || head[2] != ':')
    {
        return false;
    }
    char *ip = head + 3;
    sockaddr_in addr = to_sockaddr(ip);
    for (int i = 0; i < STORAGE_SERVERS1.size(); i++)
    {
        sockaddr_in server = STORAGE_SERVERS1[i];
        if (strcmp(inet_ntoa(server.sin_addr), inet_ntoa(addr.sin_addr)) == 0
                && server.sin_port == addr.sin_port)
        {
            index = i;
            kind = 11;
            return true;
        }
    }
    for (int i = 0; i < STORAGE_SERVERS2.size(); i++)
    {
        sockaddr_in server = STORAGE_SERVERS2[i];
        if (strcmp(inet_ntoa(server.sin_addr), inet_ntoa(addr.sin_addr)) == 0
                && server.sin_port == addr.sin_port)
        {
            index = i;
            kind = 12;
            return true;
        }
    }
    for (int i = 0; i < STORAGE_SERVERS3.size(); i++)
    {
        sockaddr_in server = STORAGE_SERVERS3[i];
        if (strcmp(inet_ntoa(server.sin_addr), inet_ntoa(addr.sin_addr)) == 0
                && server.sin_port == addr.sin_port)
        {
            index = i;
            kind = 13;
            return true;
        }
    }
    for (int i = 0; i < MAIL_SERVERS1.size(); i++)
    {
        sockaddr_in server = MAIL_SERVERS1[i];
        if (strcmp(inet_ntoa(server.sin_addr), inet_ntoa(addr.sin_addr)) == 0
                && server.sin_port == addr.sin_port)
        {
            index = i;
            kind = 21;
            return true;
        }
    }
    for (int i = 0; i < MAIL_SERVERS2.size(); i++)
    {
        sockaddr_in server = MAIL_SERVERS2[i];
        if (strcmp(inet_ntoa(server.sin_addr), inet_ntoa(addr.sin_addr)) == 0
                && server.sin_port == addr.sin_port)
        {
            index = i;
            kind = 22;
            return true;
        }
    }
    for (int i = 0; i < USER_SERVERS.size(); i++)
    {
        sockaddr_in server = USER_SERVERS[i];
        if (strcmp(inet_ntoa(server.sin_addr), inet_ntoa(addr.sin_addr)) == 0
                && server.sin_port == addr.sin_port)
        {
            index = i;
            kind = 3;
            return true;
        }
    }
    return false;
}

/* Helper function for reading from socket */
void do_read(unsigned int fd, char *buffer, string mid)
{
    char *head = buffer;
    char *tail = 0;
    while (RUNNING && (tail = strstr(buffer, "\r\n")) == NULL)
    {
        int len = strlen(buffer); // read a whole message
        int recv_len = read(fd, head, 1024 - len);
        if(recv_len == -1)
        {
            continue;
        }
        if (DEBUG)
        {
            if (recv_len > 0)
            {
                fprintf(stderr, "%s [%d] Received from %s of %d char, is %s\n",
                        debug_str().c_str(), fd, mid.c_str(), recv_len, head);
            }
        }
        while(*head != '\0')
        {
            head++; // move to next start point for read
        }
    }
}

/* Helper function for reading from socket with timeout */
void do_read_time(unsigned int fd, char *buffer, string mid)
{
    char *head = buffer;
    char *tail = 0;
    time_t now = time(NULL), pre = time(NULL);
    while (RUNNING && ((int)(now - pre) < RESPONSE_TIMEOUT) && (tail = strstr(buffer, "\r\n")) == NULL)
    {
        now = time(NULL);
        int len = strlen(buffer); // read a whole message
        int recv_len = read(fd, head, 1024 - len);
        if(recv_len == -1)
        {
            continue;
        }
        if (DEBUG)
        {
            if (recv_len > 0)
            {
                fprintf(stderr, "%s [%d] Received from %s of %d char, is %s\n",
                        debug_str().c_str(), fd, mid.c_str(), recv_len, head);
            }
        }
        while(*head != '\0')
        {
            head++; // move to next start point for read
        }
    }
}

/* Helper function for create a connection socket */
unsigned int get_socket()
{
    unsigned int out;
    if ((out = socket(PF_INET, SOCK_STREAM, 0)) == -1) // create socket to connect back-end server
    {
        fprintf(stderr, "Master: Socket open error.\n");
        exit(1);
    }
    int reuse = 1;
    if (setsockopt(out, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse,
                   sizeof(int)) == -1)
    {
        fprintf(stderr, "Master: Socket set error.\n");
        exit(1);
    }
    set_nonblocking(out);
    return out;
}

/* Converts an ip address to a sockaddr structure */
sockaddr_in to_sockaddr(string addr)
{
    struct sockaddr_in res;
    bzero(&res, sizeof(res));
    res.sin_family = AF_INET;
    int colon_idx = addr.find(COLON);
    string IP_addr = addr.substr(0, colon_idx);
    string port = addr.substr(colon_idx + 1, addr.length());
    inet_pton(AF_INET, IP_addr.c_str(), &(res.sin_addr));
    res.sin_port = htons(atoi(port.c_str()));
    return res;
}

/* Generate the head info for debug use */
string debug_str()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    struct tm *ptm;
    ptm = localtime(&t.tv_sec);
    char time_s[32] = { };
    char ms[16] = { };
    strftime(time_s, sizeof(time_s), "%H:%M:%S", ptm);
    sprintf(ms, ".%06ld", t.tv_sec);
    strcat(time_s, ms);
    return string(time_s) + " ";
}

/* Set nonblocking read() function */
void set_nonblocking(unsigned int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("Master: fcntl(F_GETFL) failed.\n");
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("Master: fcntl(F_SETFL) failed.\n");
        exit(1);
    }
}

/* A thread for recycling the threads */
void *recycle_t(void *p)
{
    while(RUNNING)
    {
        if(THREADS.size() > mem_limit) // exceed memory limit, clear finished items in vector
        {
            while(RUNNING && FINISHED.size() != 0)
            {
                for(int i = 0; i < SOCKETS.size(); i++)
                {
                    if(SOCKETS[i] == FINISHED[0])
                    {
                        pthread_join(THREADS[i], NULL);
                        SOCKETS.erase(SOCKETS.begin() + i);
                        THREADS.erase(THREADS.begin() + i);
                        FINISHED.erase(FINISHED.begin());
                        break;
                    }
                }
            }
        }
    }
}

/* Signal handler for ctrl-c */
void sig_handler(int arg)
{
    RUNNING = false;
    close(listen_fd);
    pthread_join(recycle, NULL);
    for (int i = 0; i < SOCKETS.size(); i++)
    {
        close(SOCKETS[i]);
    }
    for (int i = 0; i < THREADS.size(); i++)
    {
        pthread_join(THREADS[i], NULL);
    }
    SOCKETS.clear();
    THREADS.clear();
    FINISHED.clear();
    if (DEBUG)
    {
        printf("\nMaster node socket closed\n");
    }
}
