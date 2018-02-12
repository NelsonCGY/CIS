#include "mail.hh"

int all;

int main(int argc, char *argv[])
{
    int verbose = 0;
    all = 1;
    int topt;
    while ((topt = getopt(argc, argv, "p:tcv")) != -1)
    {
        switch (topt)
        {
        case 'v':
            verbose = 1;
            printf("Debug mode ON.\n");
            break;
        case 't':
            all = 0;
            printf("Test Mode Launching...\n");
            break;
        case 'c':
            fprintf(stderr, "Team 12\n");
            exit(1);
            break;
        case '?':
            fprintf(stderr, "Invalid input.\n");
            exit(1);
            break;
        }
    }

    if(all)
    {
        printf("Full System Launching...\n");
    }

    std::string backendConfigFile = "backend.txt";
    std::string frontendConfigFile = "frontend.txt";

    // Parse configuration file
    std::ifstream configFile(backendConfigFile);
    if (!configFile.is_open())
    {
        std::cout << "Backend configuration file does not exist." << std::endl;
        exit(EXIT_FAILURE);
    }
    else
    {
        if (verbose) std::cout << "\nBackend addresses:" << endl;
    }
    std::string line;
    std::string currentSetting = "NONE";
    std::vector<sockaddr_in> vec_masterServer;
    std::vector<sockaddr_in> vec_storageServer;
    std::vector<sockaddr_in> vec_mailServer;
    std::vector<sockaddr_in> vec_userServer;
    while (std::getline(configFile, line))
    {
        if (line.substr(0, 6) == "MASTER")
        {
            currentSetting = "MASTER";
        }
        else if (line.substr(0, 7) == "STORAGE")
        {
            currentSetting = "STORAGE";
        }
        else if (line.substr(0, 4) == "MAIL")
        {
            currentSetting = "MAIL";
        }
        else if (line.substr(0, 4) == "USER")
        {
            currentSetting = "USER";
        }
        else
        {
            if (currentSetting == "MASTER")
            {
                vec_masterServer.push_back(Str2SockAddr(line));
            }
            else if (currentSetting == "STORAGE")
            {
                vec_storageServer.push_back(Str2SockAddr(line));
            }
            else if (currentSetting == "MAIL")
            {
                vec_mailServer.push_back(Str2SockAddr(line));
            }
            else if (currentSetting == "USER")
            {
                vec_userServer.push_back(Str2SockAddr(line));
            }
            else
            {
                std::cout << "Backend configuration file has bad format." << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }

    if (verbose)
    {
        std::cout << "Master" << std::endl;
        for (auto addr : vec_masterServer)
        {
            std::cout << SockAddr2Str(addr) << std::endl;
        }
        std::cout << "STORAGE" << std::endl;
        for (auto addr : vec_storageServer)
        {
            std::cout << SockAddr2Str(addr) << std::endl;
        }
        std::cout << "MAIL" << std::endl;
        for (auto addr : vec_mailServer)
        {
            std::cout << SockAddr2Str(addr) << std::endl;
        }
        std::cout << "USER" << std::endl;
        for (auto addr : vec_userServer)
        {
            std::cout << SockAddr2Str(addr) << std::endl;
        }
    }

    std::ifstream configFileFront(frontendConfigFile);
    if (!configFileFront.is_open())
    {
        std::cout << "Frontend configuration file does not exist." << std::endl;
        exit(EXIT_FAILURE);
    }
    else
    {
        if (verbose) std::cout << "\nFrontend addresses:" << endl;
    }

    currentSetting = "NONE";
    std::vector<sockaddr_in> vec_storageFrontServer;
    std::vector<sockaddr_in> vec_mailFrontServer;
    std::vector<sockaddr_in> vec_userFrontServer;
    while (std::getline(configFileFront, line))
    {
        if (line.substr(0, 7) == "STORAGE")
        {
            currentSetting = "STORAGE";
        }
        else if (line.substr(0, 4) == "MAIL")
        {
            currentSetting = "MAIL";
        }
        else if (line.substr(0, 4) == "USER")
        {
            currentSetting = "USER";
        }
        else
        {
            if (currentSetting == "STORAGE")
            {
                vec_storageFrontServer.push_back(Str2SockAddr(line));
            }
            else if (currentSetting == "MAIL")
            {
                vec_mailFrontServer.push_back(Str2SockAddr(line));
            }
            else if (currentSetting == "USER")
            {
                vec_userFrontServer.push_back(Str2SockAddr(line));
            }
            else
            {
                std::cout << "Frontend configuration file has bad format." << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }

    std::vector<std::string> vec_shellCommand;
    std::string verboseStr = "";
    std::string verboseStr2 = " -v";
    if (verbose) verboseStr = " -v";

    if (verbose) std::cout << "STORAGE" << std::endl;
    for (auto addr : vec_storageFrontServer)
    {
        if (verbose) std::cout << SockAddr2Str(addr) << std::endl;
        vec_shellCommand.push_back("./storage " + backendConfigFile +
                                   " -p " + std::to_string(htons(addr.sin_port)) + verboseStr);
    }
    if (verbose) std::cout << "MAIL" << std::endl;
    for (auto addr : vec_mailFrontServer)
    {
        if (verbose) std::cout << SockAddr2Str(addr) << std::endl;
        vec_shellCommand.push_back("./mail " + backendConfigFile +
                                   " -p " + std::to_string(htons(addr.sin_port)) + verboseStr);
    }
    if (verbose) std::cout << "USER" << std::endl;
    for (auto addr : vec_userFrontServer)
    {
        if (verbose) std::cout << SockAddr2Str(addr) << std::endl;
        vec_shellCommand.push_back("./httpServer " + backendConfigFile +
                                   " -p " + std::to_string(htons(addr.sin_port)) + verboseStr);
    }

    for (int port = 8000; port <= 10000; port += 1000)
    {
        vec_shellCommand.push_back("./loadbalancer " + frontendConfigFile + " -p " + std::to_string(port) + verboseStr);
    }
    vec_shellCommand.push_back("./admin " + backendConfigFile + " -p 8080" + verboseStr);
    vec_shellCommand.push_back("./masterNode " + backendConfigFile + verboseStr);
    vec_shellCommand.push_back("hold");


    std::thread t[MAX_THREAD_NUM];

    if(all)
    {
        int backendServerNum = vec_storageServer.size() + vec_mailServer.size() + vec_userServer.size();
        for (int i = 0; i <= 15; i += 3)
        {
            vec_shellCommand.push_back("./keyValueStore " +
                                       backendConfigFile + " " + std::to_string(i) + verboseStr);
        }
        for (int i = 1; i <= 16; i += 3)
        {
            vec_shellCommand.push_back("./keyValueStore " +
                                       backendConfigFile + " " + std::to_string(i) + verboseStr);
        }
        for (int i = 2; i <= 17; i += 3)
        {
            vec_shellCommand.push_back("./keyValueStore " +
                                       backendConfigFile + " " + std::to_string(i) + verboseStr);
        }
    }

    std::cout << "\nStarting..." << endl;
    for (int i = 0; i < vec_shellCommand.size(); ++i)
    {
        if(vec_shellCommand[i] == "hold")
        {
            sleep(3);
            continue;
        }
        t[i] = std::thread(system, vec_shellCommand[i].c_str());
        std::cout << "[Shell command]" << vec_shellCommand[i] << std::endl;
        sleep(1);
    }

    printf("\nSystem start finished.\n");

    for (int i = 0; i < vec_shellCommand.size(); ++i)
    {
        t[i].join();
    }

    return 0;
}

