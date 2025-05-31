#ifndef NETWORK_RECEIVER_H
#define NETWORK_RECEIVER_H

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

class NetworkReceiver {
public:
    NetworkReceiver(unsigned short listenPort);
    ~NetworkReceiver();
    bool start();
    void stop();
    std::vector<unsigned char> receivePacketBlocking();

private:
#ifdef _WIN32
    SOCKET sockfd;
#else
    int sockfd;
#endif
    unsigned short listenPort_;
    bool initialized;
    bool running;
};

#endif // NETWORK_RECEIVER_H