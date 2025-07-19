

#pragma once

#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef int socklen_t;
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#endif




class NetworkReceiverMulticast {
public:
    NetworkReceiverMulticast(const std::string& multicastGroup, unsigned short port);
    ~NetworkReceiverMulticast();
    bool receivePacket(std::vector<unsigned char>& data);

private:
    #ifdef _WIN32
        SOCKET sockfd;
    #else
        int sockfd;
    #endif
    bool initialized;
    struct sockaddr_in localAddr {};
    struct ip_mreq multicastReq {};
};
