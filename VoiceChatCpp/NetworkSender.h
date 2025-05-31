#ifndef NETWORK_SENDER_H
#define NETWORK_SENDER_H



#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS // Only if you want to suppress inet_addr warning
#include <winsock2.h>
#include <ws2tcpip.h>       // Needed for InetPton and other modern WinSock APIs
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept> // For std::runtime_error etc.



class NetworkSender {
public:
    NetworkSender(const std::string& targetIp, unsigned short targetPort);
    ~NetworkSender();
    bool sendPacket(const std::vector<unsigned char>& data);

private:
#ifdef _WIN32
    SOCKET sockfd;
    SOCKADDR_IN serverAddr;
    std::string targetIp;
    int targetPort;
#else
    int sockfd;
    sockaddr_in serverAddr;
#endif
    bool initialized;
};

#endif // NETWORK_SENDER_H