/**
key Value Store minimum implementation
first argument: serverlist configuration file name eg:serverlist.txt
second argument: server Index starting from the address right below STORAGE with 0
ending at USER
-v option for debug mode
example: ./keyValueStore serverlist.txt 0 -v
commands          possible responses(all responses ending with \r\n)
/PUT r c v          +OK, -ERR
/GET r c            value,  -ERR
/CPUT r c v1 v2     +OK, -ERR
/DELETE r c         +OK, -ERR
/ADM startIndex endIndex   +OK #row1 col1 111#row2 col2 222, -ERR
(/ADM returns key value pairs starting with startIndex and ending with endIndex)
/*****************************************************************
/CHECK              +GOOD
/PRIME              +ROGER
requests                                    possible answers
#S:[IP:port number] +GOOD                   None (heart beat)
#S:[IP:port number] /CURP                   +OKCPR [prime index]
#S:[IP:port number] /NEWP [prime index]     +OKNPR [prime index]
*****************************************************************//*
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <map>
#include <tuple>
#include <algorithm>
#include <set>
#include <signal.h>
#include <pthread.h>
/************************/
#include <fcntl.h>
/**************************/
using namespace std;

//keyValue Storage
map<string, vector<tuple<string, string>>> keyValue;

//server configuration
const string COMMA = ",";
const string COLON = ":";
/************************************/
const string MASTER = "MASTER";
/*******************************************/
const string STORAGE1 = "STORAGE1";
const string STORAGE2 = "STORAGE2";
const string STORAGE3 = "STORAGE3";
const string MAIL1 = "MAIL1";
const string MAIL2 = "MAIL2";
const string USER = "USER";


bool isDebugMode = false;
string fileName;
/*******************************/
sockaddr_in master_addr;
pthread_t beat_thread;
pthread_t recycle;
int BEAT_T = 100;
int CONN_OUT_T = 1;
int mem_limit = 10;
bool RUNNING;
/**********************************/

int serverIndex = 0;
string serverIP;
string serverPort;
string serverTablet;
class Server
{
public:
    string bindAddr;
    string port;
    int serverIndex;
    string tablet;
    Server() {}
    Server(string ba, string pt, int si, string t)
    {
        bindAddr = ba;
        port = pt;
        serverIndex = si;
        tablet = t;
    }
};
vector<Server> serverVec;

void readServerList()
{
    string line;
    ifstream configFile;
    configFile.open(fileName, ios::in);
    bool startRead = false;
    string tablet = "";
    int serverIdx = 0;
    if(configFile.is_open())
    {
        while(getline(configFile, line))
        {
            if(startRead == true)
            {
                if(line == STORAGE2)
                {
                    tablet = STORAGE2;
                }
                else if(line == STORAGE3)
                {
                    tablet = STORAGE3;
                }
                else if(line == MAIL1)
                {
                    tablet = MAIL1;
                }
                else if(line == MAIL2)
                {
                    tablet = MAIL2;
                }
                else if(line == USER)
                {
                    tablet = USER;
                }
                else
                {
                    int colonIndex = line.find(COLON);
                    string ba = line.substr(0, colonIndex);
                    string pt = line.substr(colonIndex + 1, line.length());
                    Server newServer(ba, pt, serverIdx++, tablet);
                    serverVec.push_back(newServer);
                }
            }
            if(line == STORAGE1)
            {
                startRead = true;
                tablet = STORAGE1;
            }
            /***************************************/
            if(line == MASTER)
            {
                getline(configFile, line);
                int colonIndex = line.find(COLON);
                string ip = line.substr(0, colonIndex);
                string pt = line.substr(colonIndex + 1, line.length());
                bzero(&master_addr, sizeof(master_addr));
                master_addr.sin_family = AF_INET;
                inet_pton(AF_INET, ip.c_str(), &(master_addr.sin_addr));
                master_addr.sin_port = htons(atoi(pt.c_str()));
            }
            /***************************************/
        }
    }
    configFile.close();
}
//replica
bool isPrimary = false;
map<string, int> originReplicaServerindexMap;//primary use only
int primaryReplicaServerIndex;
string primaryReplicaIP;
string primaryReplicaPort;
string primaryReplicaTablet;
int maxReplicaIndex = 0;
map<string, int> replicaOkNumMap;
map<string, int> srcfdMap;
int tabletServerNum = 0;

//fault tolerance
int checkPointIndex = 0;
int checkPointCount = 0;
ofstream keyValuesLog;
ifstream keyValuesLogIn;
ofstream recoverLog;
ifstream recoverLogIn;
const string CHECKPOINT = "CHECKPOINT";

//Response message
char unknownCommand[] = "-ERR Unknown command\r\n";
char okResponse[] = "+OK\r\n";
char errResponse[] = "-ERR\r\n";
char goodResponse[] = "+GOOD\r\n";
/*********************************/
char primeResponse[] = "+ROGER\r\n";
string ADDR_HEAD = "#S:";
const string CURP = " /CURP\r\n";
const string NEWP = " /NEWP ";
/********************************/
// char replicaOkResponse[] = "+ReplicaOK\r\n";

//read command index
int readStartIndex = 0;
int commandEndIndex = 0;
int readEndIndex = 1;

int listen_fd;
//thread stuff
vector<pthread_t> threadVec; //store all the threads
vector<struct thread_data *> threadInputVec;
set<int> openFd;
/*************************/
vector<int> fdArray;
vector<int> fdFinished;
/*************************/
/**
 * thread input struct
 */
struct thread_data
{
    int comm_fd;
    struct sockaddr_in clientaddr;
};

struct do_read_input
{
    vector<int> *commandEndIndexVec;
    int *readStartIndex;
    int *commandEndIndex;
    int *readEndIndex;
};

void do_read(int fd, char *buf, int len, struct do_read_input readIndex)
{
    int rcvd = *(readIndex.readStartIndex);
    bool isFoundCommand = false;
    while(rcvd < len)
    {
        int n = read(fd, &buf[rcvd], len - rcvd);
        if(n < 0)
        {
            fprintf(stderr, "Reading Error!\n");
            exit(1);
        }
        for(int i = rcvd; i < rcvd + n - 1; i++)
        {
            if(buf[i] == '\r' && buf[i + 1] == '\n')
            {
                *(readIndex.commandEndIndex) = i + 1;
                readIndex.commandEndIndexVec->push_back(*(readIndex.commandEndIndex));
                isFoundCommand = true;
            }
        }
        if(isFoundCommand)
        {
            *(readIndex.readEndIndex) = rcvd + n;
            return;
        }
        rcvd += n;
    }
}

bool do_write(int fd, const char *buf, int len)
{
    int sent = 0;
    while(sent < len)
    {
        int n = write(fd, &buf[sent], len - sent);
        if(n < 0)
        {
            return false;
        }
        sent += n;
    }
    return true;
}

/**********************************************/
void get_new_prime()
{
    string request = ADDR_HEAD + NEWP + to_string(primaryReplicaServerIndex) + "\r\n";
    const char *req = request.c_str();
    int oldIndex = primaryReplicaServerIndex;
    int out = socket(PF_INET, SOCK_STREAM, 0);
    if(out < 0)
    {
        fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
        exit(1);
    }
    int enable = 1;
    if(setsockopt(out, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        fprintf(stderr, "port reusing failed");
        exit(1);
    }
    int ret = connect(out, (struct sockaddr *) &master_addr, sizeof(master_addr));
    if(ret != -1)
    {
        do_write(out, req, strlen(req));
        if(isDebugMode)
        {
            cout << "Request new primary: " << request;
        }
    }
    close(out);
    time_t now_time = time(NULL), pre_time = time(NULL), st_time = time(NULL);
    while(RUNNING && primaryReplicaServerIndex == oldIndex)
    {
        now_time = time(NULL);
        if(now_time - pre_time >= BEAT_T)
        {
            if(isDebugMode)
            {
                cout << "Waiting for new primary..." << endl;
            }
            pre_time = now_time;
        }
        if(now_time - st_time >= 10)
        {
            if(isDebugMode)
            {
                cout << "What happened? No primary?" << endl;
            }
            return;
        }
    }
}
/******************************************************/

void replicaWrite(string writeMsg, bool toPrimary)
{
    if(!toPrimary)
    {
        for(int i = 0; i < serverVec.size(); i++)
        {
            int destServerIndex = serverVec[i].serverIndex;
            string destServerTablet = serverVec[i].tablet;
            if(destServerTablet == primaryReplicaTablet)
            {
                int sockfd = socket(PF_INET, SOCK_STREAM, 0);
                if(sockfd < 0)
                {
                    fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
                    exit(1);
                }
                int enable = 1;
                if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                {
                    fprintf(stderr, "port reusing failed");
                    exit(1);
                }
                string destServerIP = serverVec[i].bindAddr;
                string destServerPort = serverVec[i].port;
                struct sockaddr_in dest;
                bzero(&dest, sizeof(dest));
                dest.sin_family = AF_INET;
                dest.sin_port = htons(atoi(destServerPort.c_str()));
                inet_pton(AF_INET, destServerIP.c_str(), &(dest.sin_addr));
                int ret = connect(sockfd, (struct sockaddr *)&dest, sizeof(dest));
                if(ret != -1)
                {
                    char serverSendChar[1000000] = {};
                    strcpy(serverSendChar, writeMsg.c_str());
                    do_write(sockfd, serverSendChar, strlen(serverSendChar));
                    close(sockfd);
                    if(isDebugMode)
                    {
                        cout << "Replica write to server: " << destServerIndex << " " << writeMsg;
                    }
                }
            }
        }
    }
    else
    {
        /***************************************/
        int ret = -1, count = 0;
        while(RUNNING && ret == -1)
        {
            /********************************/
            int sockfd = socket(PF_INET, SOCK_STREAM, 0);
            if(sockfd < 0)
            {
                fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
                exit(1);
            }
            int enable = 1;
            if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
            {
                fprintf(stderr, "port reusing failed");
                exit(1);
            }
            string destServerIP = serverVec[primaryReplicaServerIndex].bindAddr;
            string destServerPort = serverVec[primaryReplicaServerIndex].port;
            struct sockaddr_in dest;
            bzero(&dest, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(atoi(destServerPort.c_str()));
            inet_pton(AF_INET, destServerIP.c_str(), &(dest.sin_addr));
            /***********************************/
            ret = connect(sockfd, (struct sockaddr *)&dest, sizeof(dest));
            if(ret == -1)
            {
                count++;
                if(count == 10)
                {
                    if(isDebugMode)
                    {
                        cout << "Can't find any primary! I am working! Why not me! Fatal error!" << endl;
                    }
                    exit(1);
                }
                get_new_prime();
                continue;
            }/***************************/
            char serverSendChar[1000000] = {};
            strcpy(serverSendChar, writeMsg.c_str());
            do_write(sockfd, serverSendChar, strlen(serverSendChar));
            close(sockfd);
            if(isDebugMode)
            {
                cout << "Replica write to primary: " << primaryReplicaServerIndex << " " << writeMsg;
            }/**********************/
        }/**************************/
    }
}

void recoverFromLog()
{
    string writeMsg = "RCOV " + to_string(serverIndex) + "\r\n";
    bool toPrimary = true;
    replicaWrite(writeMsg, toPrimary);
}

void *threadFunc(void *threadInput)
{
    struct thread_data *input = (struct thread_data *)threadInput;
    struct sockaddr_in clientaddr = input->clientaddr;
    int readStartIndex = 0;
    int commandEndIndex = 0;
    int readEndIndex = 1;
    vector<int> commandEndIndexVec;

    struct do_read_input readIndex;
    readIndex.readStartIndex = &readStartIndex;
    readIndex.commandEndIndex = &commandEndIndex;
    readIndex.readEndIndex = &readEndIndex;
    readIndex.commandEndIndexVec = &commandEndIndexVec;

    char request[1000000] = {};
    for(int i = commandEndIndex + 1; i < readEndIndex; i++)
    {
        request[i - commandEndIndex - 1] = request[i];
    }
    readStartIndex = readEndIndex - commandEndIndex - 1;
    int len = 1000000 - readStartIndex;
    do_read(input->comm_fd, request, len, readIndex);
    int curStart = 0;

    for(int k = 0; k < commandEndIndexVec.size(); k++)
    {
        int curcommandEndIndex = commandEndIndexVec[k];
        if(k > 0)
        {
            curStart = commandEndIndexVec[k - 1] + 1;
            curcommandEndIndex = curcommandEndIndex - commandEndIndexVec[k - 1] - 1;
        }
        char command[curcommandEndIndex + 1] = {};
        for(int i = curStart; i < curStart + curcommandEndIndex; i++)
        {
            command[i - curStart] = request[i];
        }
        if(curcommandEndIndex < 4)
        {
            fprintf(stderr, "Unknown Command\n");
        }
        else
        {
            char cmd[5];
            for(int i = 0; i < 4; i++)
            {
                cmd[i] = command[i];
            }
            string cmdStr(cmd);
            if(cmdStr == "/PUT")
            {
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: PUT %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                string writeMsg = "GPUT " + recvStr + " " + to_string(serverIndex) + "\r\n";
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string columnKey = recvStr.substr(0, spaceIndex);
                string value = recvStr.substr(spaceIndex + 1, recvStr.length());
                string replicaKey = rowKey + columnKey + value;
                srcfdMap[replicaKey] = input->comm_fd;
                bool toPrimary = true;
                replicaWrite(writeMsg, toPrimary);
                commandEndIndexVec.clear();
                return NULL;
            }
            else if(cmdStr == "GPUT")
            {
                //Only primary can get GPUT
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: GPUT %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                string writeMsg = "RPUT " + recvStr + " " + to_string(checkPointIndex) + " " + to_string(checkPointCount) + "\r\n";
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string columnKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string value = recvStr.substr(0, spaceIndex);
                string fromServerIndex = recvStr.substr(spaceIndex + 1, recvStr.length());
                string replicaKey = rowKey + columnKey + value;
                originReplicaServerindexMap[replicaKey] = stoi(fromServerIndex);
                if(!isPrimary)
                {
                    cout << "It is impossible" << endl;
                }
                else
                {
                    bool toPrimary = false;
                    replicaWrite(writeMsg, toPrimary);
                }
            }
            else if(cmdStr == "RPUT")
            {
                //All node should get RPUT for each replica. Only primary sends RPUT
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: RPUT %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                string writeMsg = "+ROK " + recvStr + "\r\n";
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string columnKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string value = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string fromServerIndex = recvStr.substr(0, spaceIndex);
                //update the checkpointindex and checkpoint
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string cpIndex = recvStr.substr(0, spaceIndex);
                string cpCount = recvStr.substr(spaceIndex + 1, recvStr.length());
                checkPointIndex = stoi(cpIndex);
                checkPointCount = stoi(cpCount);
                //once getting RPUT, store directly!
                tuple<string, string> element = make_tuple(columnKey, value);
                if(keyValue.find(rowKey) == keyValue.end())
                {
                    vector<tuple<string, string>> newestTupleVec;
                    newestTupleVec.push_back(element);
                    keyValue[rowKey] = newestTupleVec;
                }
                else
                {
                    bool findColumn = false;
                    vector<tuple<string, string>> *curTupleVec = &(keyValue[rowKey]);
                    for(int i = 0; i < curTupleVec->size(); i++)
                    {
                        tuple<string, string> *curTuple = &(*curTupleVec)[i];
                        if(get<0>(*curTuple) == columnKey)
                        {
                            findColumn = true;
                            if(value == "&")
                            {
                                (*curTupleVec).erase(curTupleVec->begin() + i);
                            }
                            else
                            {
                                string *valueOri = &(get<1>(*curTuple));
                                *valueOri = value;
                            }
                        }
                    }
                    if(findColumn == false)
                    {
                        tuple<string, string> element = make_tuple(columnKey, value);
                        curTupleVec->push_back(element);
                    }
                }
                bool toPrimary = true;
                replicaWrite(writeMsg, toPrimary);
            }
            else if(cmdStr == "+ROK")
            {
                //Only primary can get +ROK
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: +ROK %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                string writeMsg = "+RFH " + recvStr + "\r\n";
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string columnKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string value = recvStr.substr(0, spaceIndex);
                string fromServerIndex = recvStr.substr(spaceIndex + 1, recvStr.length());
                string replicaKey = rowKey + columnKey + value;
                if(!isPrimary)
                {
                    cout << "It is not possible" << endl;
                    //might need lock for replicaOkNumMap
                }
                else
                {
                    if(replicaOkNumMap.find(replicaKey) == replicaOkNumMap.end())
                    {
                        replicaOkNumMap[replicaKey] = 1;
                    }
                    else
                    {
                        int *curReplicaOkNum = &(replicaOkNumMap[replicaKey]);
                        (*curReplicaOkNum)++;
                    }
                    if(replicaOkNumMap[replicaKey] == 1)
                    {
                        //write to recover log file
                        recoverLog.open("log/recoverLog" + serverTablet + ".txt", ios::app);
                        string newest = "FPUT " + rowKey + " " + columnKey + " " + value + "\r";
                        recoverLog << newest << endl;
                        recoverLog.close();

                        int originReplicaServerIndex = originReplicaServerindexMap[replicaKey];
                        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
                        if(sockfd < 0)
                        {
                            fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
                            exit(1);
                        }
                        int enable = 1;
                        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                        {
                            fprintf(stderr, "port reusing failed");
                            exit(1);
                        }
                        string destServerIP = serverVec[originReplicaServerIndex].bindAddr;
                        string destServerPort = serverVec[originReplicaServerIndex].port;
                        struct sockaddr_in dest;
                        bzero(&dest, sizeof(dest));
                        dest.sin_family = AF_INET;
                        dest.sin_port = htons(atoi(destServerPort.c_str()));
                        inet_pton(AF_INET, destServerIP.c_str(), &(dest.sin_addr));
                        connect(sockfd, (struct sockaddr *)&dest, sizeof(dest));
                        char writeMsgChar[writeMsg.length() + 1];
                        strcpy(writeMsgChar, writeMsg.c_str());
                        do_write(sockfd, writeMsgChar, strlen(writeMsgChar));
                        if(isDebugMode)
                        {
                            cout << "send out " << writeMsg << " to " << originReplicaServerIndex << endl;
                        }
                        close(sockfd);
                    }
                    if(replicaOkNumMap[replicaKey] == tabletServerNum)
                    {
                        checkPointCount++;
                        if(checkPointCount == 3)
                        {
                            checkPointIndex++;
                            keyValuesLog.open("log/keyValuesLog" + serverTablet + ".txt", ios::app);
                            map<string, vector<tuple<string, string>>>:: iterator it;
                            for(it = keyValue.begin(); it != keyValue.end(); ++it)
                            {
                                string itRowKey = it->first;
                                vector<tuple<string, string>> itVec = it->second;
                                for(int i = 0; i < itVec.size(); i++)
                                {
                                    tuple<string, string> itTuple = itVec[i];
                                    string itColumnKey = get<0>(itTuple);
                                    string itValue = get<1>(itTuple);
                                    string newest = itRowKey + " " + itColumnKey + " " + itValue;
                                    keyValuesLog << newest << endl;
                                }
                            }
                            keyValuesLog << CHECKPOINT << to_string(checkPointIndex) << endl;
                            keyValuesLog.close();
                            checkPointCount = 0;
                            string removeName = "log/recoverLog" + serverTablet + ".txt";
                            remove(removeName.c_str());
                            ofstream temp;
                            temp.open(removeName, ios::app);
                            temp.close();
                        }
                    }
                }
            }
            else if(cmdStr == "+RFH")
            {
                //only originalServer can get +RFH
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: +RFH %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                string writeMsg = "+RFH " + recvStr + "\r\n";
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string columnKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string value = recvStr.substr(0, spaceIndex);
                string fromServerIndex = recvStr.substr(spaceIndex + 1, recvStr.length());
                string replicaKey = rowKey + columnKey + value;
                int srcfd = srcfdMap[replicaKey];
                do_write(srcfd, okResponse, strlen(okResponse));
                close(srcfd);
                srcfdMap.erase(replicaKey);
            }
            else if(cmdStr == "RCOV")
            {
                //Only primary can receive this
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: RCOV %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                recoverLogIn.open("log/recoverLog" + serverTablet + ".txt", ios::in);
                keyValuesLogIn.open("log/keyValuesLog" + serverTablet + ".txt", ios::in);
                int rcovServerIndex = stoi(recvStr);
                int sockfd = socket(PF_INET, SOCK_STREAM, 0);
                if(sockfd < 0)
                {
                    fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
                    exit(1);
                }
                int enable = 1;
                if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                {
                    fprintf(stderr, "port reusing failed");
                    exit(1);
                }
                string destServerIP = serverVec[rcovServerIndex].bindAddr;
                string destServerPort = serverVec[rcovServerIndex].port;
                struct sockaddr_in dest;
                bzero(&dest, sizeof(dest));
                dest.sin_family = AF_INET;
                dest.sin_port = htons(atoi(destServerPort.c_str()));
                inet_pton(AF_INET, destServerIP.c_str(), &(dest.sin_addr));
                string line;
                while(getline(keyValuesLogIn, line))
                {
                    if(line.substr(0, 10) != "CHECKPOINT")
                    {
                        connect(sockfd, (struct sockaddr *)&dest, sizeof(dest));
                        line = "FPUT " + line + "\n";
                        if(isDebugMode)
                        {
                            cout << "recovery sending out" << line;
                        }
                        char writeMsgChar[line.length() + 1];
                        strcpy(writeMsgChar, line.c_str());
                        do_write(sockfd, writeMsgChar, strlen(writeMsgChar));
                    }
                }
                while(getline(recoverLogIn, line))
                {
                    connect(sockfd, (struct sockaddr *)&dest, sizeof(dest));
                    line = line + "\n";
                    if(isDebugMode)
                    {
                        cout << "recovery sending out " << line;
                    }
                    char writeMsgChar[line.length() + 1];
                    strcpy(writeMsgChar, line.c_str());
                    do_write(sockfd, writeMsgChar, strlen(writeMsgChar));
                }
                close(sockfd);
                recoverLogIn.close();
                keyValuesLogIn.close();
            }
            else if(cmdStr == "FPUT")
            {
                //restarted server can receive this
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: FPUT %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string columnKey = recvStr.substr(0, spaceIndex);
                string value = recvStr.substr(spaceIndex + 1, recvStr.length());
                tuple<string, string> element = make_tuple(columnKey, value);
                if(keyValue.find(rowKey) == keyValue.end())
                {
                    vector<tuple<string, string>> newestTupleVec;
                    newestTupleVec.push_back(element);
                    keyValue[rowKey] = newestTupleVec;
                }
                else
                {
                    bool findColumn = false;
                    vector<tuple<string, string>> *curTupleVec = &(keyValue[rowKey]);
                    for(int i = 0; i < curTupleVec->size(); i++)
                    {
                        tuple<string, string> *curTuple = &(*curTupleVec)[i];
                        if(get<0>(*curTuple) == columnKey)
                        {
                            findColumn = true;
                            if(value == "&")
                            {
                                (*curTupleVec).erase(curTupleVec->begin() + i);
                            }
                            else
                            {
                                string *valueOri = &(get<1>(*curTuple));
                                *valueOri = value;
                            }
                        }
                    }
                    if(findColumn == false)
                    {
                        tuple<string, string> element = make_tuple(columnKey, value);
                        curTupleVec->push_back(element);
                    }
                }
            }
            else if(cmdStr == "/GET")
            {
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: GET %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                string columnKey = recvStr.substr(spaceIndex + 1, recvStr.length());
                if(keyValue.find(rowKey) == keyValue.end())
                {
                    if(isDebugMode)
                    {
                        fprintf(stderr, "Can't find row key\n");
                    }
                    do_write(input->comm_fd, errResponse, strlen(errResponse));
                }
                else
                {
                    vector<tuple<string, string>> curTupleVec = keyValue[rowKey];
                    bool findColumnKey = false;
                    for(int i = 0; i < curTupleVec.size(); i++)
                    {
                        tuple<string, string> curTuple = curTupleVec[i];
                        if(get<0>(curTuple) == columnKey)
                        {
                            findColumnKey = true;
                            string value = get<1>(curTuple);
                            value = value + "\r\n";
                            char valueReturn[value.length() + 1] = {};
                            strcpy(valueReturn, value.c_str());
                            do_write(input->comm_fd, valueReturn, strlen(valueReturn));
                        }
                    }
                    if(findColumnKey == false)
                    {
                        do_write(input->comm_fd, errResponse, strlen(errResponse));
                        if(isDebugMode)
                        {
                            fprintf(stderr, "Can't find column key\n");
                        }
                    }
                }
            }
            else if(cmdStr == "/CPU")
            {
                char recvText[curcommandEndIndex - 5] = {};
                for(int i = 0; i < curcommandEndIndex - 7; i++)
                {
                    recvText[i] = command[i + 6];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: CPUT %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string columnKey = recvStr.substr(0, spaceIndex);
                recvStr = recvStr.substr(spaceIndex + 1, recvStr.length());
                spaceIndex = recvStr.find(" ");
                string value1 = recvStr.substr(0, spaceIndex);
                string value2 = recvStr.substr(spaceIndex + 1, recvStr.length());
                string writeMsg = "GPUT " + rowKey + " " + columnKey + " " + value2 + " " + to_string(serverIndex) + "\r\n";
                string replicaKey = rowKey + columnKey + value2;
                if(keyValue.find(rowKey) == keyValue.end())
                {
                    if(isDebugMode)
                    {
                        fprintf(stderr, "Can't find row key\n");
                    }
                    do_write(input->comm_fd, errResponse, strlen(errResponse));
                }
                else
                {
                    vector<tuple<string, string>> *curTupleVec = &keyValue[rowKey];
                    bool findColumnKey = false;
                    for(int i = 0; i < curTupleVec->size(); i++)
                    {
                        tuple<string, string> *curTuple = &((*curTupleVec)[i]);
                        if(get<0>(*curTuple) == columnKey)
                        {
                            findColumnKey = true;
                            string *valueOri = &(get<1>(*curTuple));
                            if(*valueOri == value1)
                            {
                                srcfdMap[replicaKey] = input->comm_fd;
                                bool toPrimary = true;
                                replicaWrite(writeMsg, toPrimary);
                                commandEndIndexVec.clear();
                                return NULL;
                            }
                            else
                            {
                                do_write(input->comm_fd, errResponse, strlen(errResponse));
                                if(isDebugMode)
                                {
                                    fprintf(stderr, "current value is not V1\n");
                                }
                            }
                        }
                    }
                    if(findColumnKey == false)
                    {
                        do_write(input->comm_fd, errResponse, strlen(errResponse));
                        if(isDebugMode)
                        {
                            fprintf(stderr, "Can't find column key\n");
                        }
                    }
                }
            }
            else if(cmdStr == "/DEL")
            {
                char recvText[curcommandEndIndex - 7] = {};
                for(int i = 0; i < curcommandEndIndex - 9; i++)
                {
                    recvText[i] = command[i + 8];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: DELETE %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);

                size_t spaceIndex = recvStr.find(" ");
                string rowKey = recvStr.substr(0, spaceIndex);
                string columnKey = recvStr.substr(spaceIndex + 1, recvStr.length());
                string writeMsg = "GPUT " + rowKey + " " + columnKey + " & " + to_string(serverIndex) + "\r\n";
                string replicaKey = rowKey + columnKey + "&";
                if(keyValue.find(rowKey) == keyValue.end())
                {
                    if(isDebugMode)
                    {
                        fprintf(stderr, "Can't find row key\n");
                    }
                    do_write(input->comm_fd, errResponse, strlen(errResponse));
                }
                else
                {
                    vector<tuple<string, string>> *curTupleVec = &(keyValue[rowKey]);
                    bool findColumnKey = false;
                    for(int i = 0; i < curTupleVec->size(); i++)
                    {
                        tuple<string, string> curTuple = (*curTupleVec)[i];
                        if(get<0>(curTuple) == columnKey)
                        {
                            findColumnKey = true;
                            srcfdMap[replicaKey] = input->comm_fd;
                            bool toPrimary = true;
                            replicaWrite(writeMsg, toPrimary);
                            commandEndIndexVec.clear();
                            return NULL;
                        }
                    }
                    if(findColumnKey == false)
                    {
                        do_write(input->comm_fd, errResponse, strlen(errResponse));
                        if(isDebugMode)
                        {
                            fprintf(stderr, "Can't find column key\n");
                        }
                    }
                }
            }
            else if(cmdStr == "/CHE" && command[4] == 'C' && command[5] == 'K')
            {
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: CHECK from master node\n", input->comm_fd);
                }
                do_write(input->comm_fd, goodResponse, strlen(goodResponse));
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] S: +GOOD\n", input->comm_fd);
                }
            }
            else if(cmdStr == "/ADM")
            {
                char recvText[curcommandEndIndex - 4] = {};
                for(int i = 0; i < curcommandEndIndex - 6; i++)
                {
                    recvText[i] = command[i + 5];
                }
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: ADM %s\n", input->comm_fd, recvText);
                }
                string recvStr(recvText);
                size_t spaceIndex = recvStr.find(" ");
                int startIndex = stoi(recvStr.substr(0, spaceIndex));
                int endIndex = stoi(recvStr.substr(spaceIndex + 1, recvStr.length()));
                vector<tuple<string, tuple<string, string>>> adminKeyValue;//ADMIN USE ONLY
                map<string, vector<tuple<string, string>>>:: iterator it;
                for(it = keyValue.begin(); it != keyValue.end(); ++it)
                {
                    string itRowKey = it->first;
                    vector<tuple<string, string>> itVec = it->second;
                    for(int i = 0; i < itVec.size(); i++)
                    {
                        tuple<string, string> itTuple = itVec[i];
                        tuple<string, tuple<string, string>> element = make_tuple(itRowKey, itTuple);
                        adminKeyValue.push_back(element);
                    }
                }
                if (endIndex >= adminKeyValue.size())   // admin doesn't know keyvalue size
                {
                    endIndex = adminKeyValue.size() - 1;
                }
                if(adminKeyValue.empty())
                {
                    if(isDebugMode)
                    {
                        fprintf(stderr, "keyValue is empty\n");
                    }
                    do_write(input->comm_fd, errResponse, strlen(errResponse));
                }
                else if(startIndex > endIndex || endIndex > adminKeyValue.size() - 1)
                {
                    if(isDebugMode)
                    {
                        fprintf(stderr, "/ADM index err\n");
                    }
                    do_write(input->comm_fd, errResponse, strlen(errResponse));
                }
                else
                {
                    string writeMsg = "+OK ";
                    for(int i = startIndex; i <= endIndex; i++)
                    {
                        string rowKey = get<0>(adminKeyValue[i]);
                        tuple<string, string> tempTuple = get<1>(adminKeyValue[i]);
                        string columnKey = get<0>(tempTuple);
                        string value = get<1>(tempTuple);
                        writeMsg = writeMsg + "#" + rowKey + " " + columnKey + " " + value;
                    }
                    writeMsg = writeMsg + "\r\n";
                    char writeMsgChar[writeMsg.length() + 1] = {};
                    strcpy(writeMsgChar, writeMsg.c_str());
                    do_write(input->comm_fd, writeMsgChar, strlen(writeMsgChar));
                }

            }/*****************************************/
            else if(cmdStr == "/PRI" && command[4] == 'M' && command[5] == 'E')
            {
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: PRIME from master node\n", input->comm_fd);
                }
                do_write(input->comm_fd, primeResponse, strlen(primeResponse));
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] S: +ROGER\n", input->comm_fd);
                }
            }
            else if(cmdStr == "+OKN" && command[4] == 'P' && command[5] == 'R')
            {
                int i = 7, idx = 0;
                while(command[i] != '\r' && ((command[i] - '0' <= 9) && (command[i] - '0' >= 0)))
                {
                    idx = idx * 10 + (command[i] - '0');
                    i++;
                }
                primaryReplicaServerIndex = idx;
                isPrimary = (serverIndex == primaryReplicaServerIndex) ? true : false;
                if(isDebugMode)
                {
                    fprintf(stderr, "[%d] C: +OKNPR %d from master node\nCurrent primary index: %d\n"
                            , input->comm_fd, idx, primaryReplicaServerIndex);
                    if(isPrimary)
                    {
                        fprintf(stderr, "I am the new primary!\n");
                    }
                }
            } /************************************************/
            else
            {
                do_write(input->comm_fd, errResponse, strlen(errResponse));
                if(isDebugMode)
                {
                    fprintf(stderr, "Unsupport Command\n");
                }
            }
        }
    }
    close(input->comm_fd);
    /*************************/
    fdFinished.push_back(input->comm_fd);
    /*********************************/
    commandEndIndexVec.clear();
}

/*******************************************/
void *heartbeat(void *p)
{
    string alive = ADDR_HEAD + " " + string(goodResponse);
    const char *res = alive.c_str();
    int bt = rand() % BEAT_T + 6;
    time_t now_time = time(NULL), pre_time = time(NULL);
    while(RUNNING)
    {
        now_time = time(NULL);
        if(now_time - pre_time >= bt)
        {
            int out = socket(PF_INET, SOCK_STREAM, 0);
            if(out < 0)
            {
                fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
                exit(1);
            }
            int enable = 1;
            if(setsockopt(out, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
            {
                fprintf(stderr, "port reusing failed");
                exit(1);
            }
            int ret = connect(out, (struct sockaddr *) &master_addr, sizeof(master_addr));
            if(ret != -1)
            {
                do_write(out, res, strlen(res));
                if(isDebugMode)
                {
                    cout << "Heartbeat: " << "time: " << bt << " send: " << alive;
                }
                pre_time = now_time;
            }
            close(out);
            bt = rand() % BEAT_T + 6;
        }
    }
}

void *recycle_t(void *p)
{
    while(RUNNING)
    {
        if(threadVec.size() > mem_limit) // exceed memory limit, clear finished items in vector
        {
            while(RUNNING && fdFinished.size() != 0)
            {
                for(int i = 0; i < fdArray.size(); i++)
                {
                    if(fdArray[i] == fdFinished[0])
                    {
                        pthread_join(threadVec[i], NULL);
                        fdArray.erase(fdArray.begin() + i);
                        threadVec.erase(threadVec.begin() + i);
                        fdFinished.erase(fdFinished.begin());
                        break;
                    }
                }
            }
        }
    }
}

void ask_primary()
{
    string ask = ADDR_HEAD + CURP;
    const char *req = ask.c_str();
    int out = socket(PF_INET, SOCK_STREAM, 0);
    if(out < 0)
    {
        fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
        exit(1);
    }
    int ret = connect(out, (struct sockaddr *) &master_addr, sizeof(master_addr));
    if(ret != -1)
    {
        do_write(out, req, strlen(req));
        char buffer[257] = {};
        char *head = buffer;
        char *tail = 0;
        while ((tail = strstr(buffer, "\r\n")) == NULL)
        {
            int len = strlen(buffer); // read a whole message
            int recv_len = read(out, head, 256 - len);
            if(recv_len == -1)
            {
                continue;
            }
            while(*head != '\0')
            {
                head++; // move to next start point for read
            }
        }
        char command[7] = {};
        for (int i = 0; i < 6; i++)
        {
            command[i] = buffer[i];
        }
        if(strcmp(command, "+OKCPR") == 0)
        {
            int i = 7, idx = 0;
            while(buffer[i] != '\r' && ((buffer[i] - '0' <= 9) && (buffer[i] - '0' >= 0)))
            {
                idx = idx * 10 + (buffer[i] - '0');
                i++;
            }
            primaryReplicaServerIndex = idx;
            if(isDebugMode)
            {
                cout << "Got current primary: " << primaryReplicaServerIndex << endl;
            }
            close(out);
            return;
        }
    }

    fprintf(stderr, "Can't get primary from master.\n");
    close(out);
    exit(1);
}
/***********************************************/

void signalHandler(int s)
{
    if(s == 2)
    {
        /***********************************/
        RUNNING = false;
        close(listen_fd);
        pthread_join(beat_thread, NULL);
        pthread_join(recycle, NULL);
        /***************************/
        set<int>::iterator it;
        for(it = openFd.begin(); it != openFd.end(); it++)
        {
            close(*it);
        }
        void *status;
        for(int i = 0; i < threadVec.size(); i++)
        {
            pthread_join((pthread_t)(threadVec[i]), &status);
        }
        exit(0);
    }
}


int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stderr, "*** Author: Haoran Shao (haorshao)\n");
        exit(1);
    }
    int result;
    while((result = getopt(argc, argv, "v")) != -1)
    {
        switch(result)
        {
        case 'v':
            isDebugMode = true;
            break;
        default:
            break;
        }
    }
    argc -= optind;
    argv += optind;
    if(argc < 2)
    {
        fprintf(stderr, "Invalid Input\n");
        exit(1);
    }
    else
    {
        fileName = argv[0];
        serverIndex = atoi(argv[1]);
    }
    readServerList();
    if(serverIndex >= serverVec.size())
    {
        fprintf(stderr, "serverIndex out of range\r\n");
        exit(1);
    }

    serverIP = serverVec[serverIndex].bindAddr;
    serverPort = serverVec[serverIndex].port;
    serverTablet = serverVec[serverIndex].tablet;
    /*************************************/
    ADDR_HEAD += serverIP + ":" + serverPort;
    ask_primary();
    /************************************/

    if(serverIndex == primaryReplicaServerIndex)
    {
        isPrimary = true;
    }
    primaryReplicaIP = serverVec[primaryReplicaServerIndex].bindAddr;
    primaryReplicaPort = serverVec[primaryReplicaServerIndex].port;
    primaryReplicaTablet = serverVec[primaryReplicaServerIndex].tablet;
    if(serverTablet != primaryReplicaTablet)
    {
        fprintf(stderr, "primary selection error\r\n");
        exit(1);
    }
    for(int i = 0; i < serverVec.size(); i++)
    {
        if(serverVec[i].tablet == primaryReplicaTablet)
        {
            tabletServerNum++;
        }
    }

    if(isDebugMode)
    {
        /**************************/
        cout << "Current Address: " << serverIP << ":" << serverPort << " Index: " << serverIndex << endl;
    }/*********************/

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0)
    {
        fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
        exit(1);
    }
    int enable = 1;
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        fprintf(stderr, "port reusing failed");
        exit(1);
    }

    openFd.insert(listen_fd);

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &(servaddr.sin_addr));
    servaddr.sin_port = htons(atoi(serverPort.c_str()));
    bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(listen_fd, 10);

    /***********************************/
    RUNNING = true;
    //recover
    recoverFromLog();
    srand(time(0));
    pthread_create(&beat_thread, NULL, &heartbeat, NULL);
    pthread_create(&recycle, NULL, &recycle_t, NULL);
    /***************************/

    while(RUNNING)
    {
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        int comm_fd = accept(listen_fd, (struct sockaddr *) &clientaddr, &clientaddrlen);
        if(isDebugMode)
        {
            printf("Connection from %s:%d\n", inet_ntoa(clientaddr.sin_addr), htons(clientaddr.sin_port));
        }
        openFd.insert(comm_fd);
        pthread_t newThread;
        threadVec.push_back(newThread);
        struct thread_data *threadInput = (struct thread_data *)malloc(sizeof(struct thread_data));
        threadInput->comm_fd = comm_fd;
        threadInput->clientaddr = clientaddr;
        threadInputVec.push_back(threadInput);
        int r = 0;
        r = pthread_create(&(threadVec[threadVec.size() - 1]), NULL, threadFunc, threadInput);
        /***************************/
        fdArray.push_back(comm_fd);
        /***********************/
    }

    return 0;
}
