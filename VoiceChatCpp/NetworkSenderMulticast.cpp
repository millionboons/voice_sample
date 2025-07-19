#include "NetworkSenderMulticast.h"

NetworkSenderMulticast::NetworkSenderMulticast(const std::string& multicastIp, unsigned short multicastPort) : initialized(false) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return;
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return;
    }
#else
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return;
    }
#endif

    // Set multicast TTL (Time To Live)
    unsigned char ttl = 1;  // Restrict to local network
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl)) < 0) {
        perror("Setting IP_MULTICAST_TTL failed");
        return;
    }

    // Set loopback (optional)
    unsigned char loopback = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopback, sizeof(loopback)) < 0) {
        perror("Setting IP_MULTICAST_LOOP failed");
        return;
    }

    // Prepare multicast address
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(multicastPort);
#ifdef _WIN32
    if (InetPtonA(AF_INET, multicastIp.c_str(), &(multicastAddr.sin_addr)) != 1) {
        std::cerr << "Invalid multicast IP address.\n";
        WSACleanup();
        return;
    }
#else
    if (inet_pton(AF_INET, multicastIp.c_str(), &(multicastAddr.sin_addr)) != 1) {
        perror("Invalid multicast IP address");
        return;
    }
#endif

    initialized = true;
}

NetworkSenderMulticast::~NetworkSenderMulticast() {
    if (initialized) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
    }
}

bool NetworkSenderMulticast::sendPacket(const std::vector<unsigned char>& data) {
    if (!initialized) return false;

#ifdef _WIN32
    int bytesSent = sendto(sockfd, (const char*)data.data(), static_cast<int>(data.size()), 0,
        (SOCKADDR*)&multicastAddr, sizeof(multicastAddr));
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
        return false;
    }
#else
    ssize_t bytesSent = sendto(sockfd, data.data(), static_cast<int>(data.size()), 0,
        (struct sockaddr*)&multicastAddr, sizeof(multicastAddr));
    if (bytesSent < 0) {
        perror("sendto failed");
        return false;
    }
#endif
    return (size_t)bytesSent == data.size();
}
