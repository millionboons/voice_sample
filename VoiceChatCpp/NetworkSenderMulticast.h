#pragma once
#include <vector>
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <iostream>
#include <ws2tcpip.h>

class NetworkSenderMulticast {
public:
    NetworkSenderMulticast(const std::string& multicastIp, unsigned short multicastPort);
    ~NetworkSenderMulticast();
    bool sendPacket(const std::vector<unsigned char>& data);

private:
    #ifdef _WIN32
        SOCKET sockfd ;
    #else
        int sockfd;
    #endif
    struct sockaddr_in multicastAddr;
    bool initialized;
};
