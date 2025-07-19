#include "NetworkReceiverMulticast.h"
#include <Ws2tcpip.h> // Add this include for InetPton

NetworkReceiverMulticast::NetworkReceiverMulticast(const std::string& multicastGroup, unsigned short port) : initialized(false) {
#ifdef _WIN32
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return;
    }
#endif
        
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return;
    }

    // Reuse address
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(port);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        perror("Bind failed");
        return;
    }

    // Join multicast group
    struct in_addr addr;
    if (InetPtonA(AF_INET, multicastGroup.c_str(), &addr) != 1) {
        std::cerr << "Invalid multicast address.\n";
        return;
    }
    multicastReq.imr_multiaddr.s_addr = addr.s_addr;
    multicastReq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&multicastReq, sizeof(multicastReq)) < 0) {
        perror("Multicast join failed");
        return;
    }

    initialized = true;
}

NetworkReceiverMulticast::~NetworkReceiverMulticast() {
    if (initialized) {
        setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&multicastReq, sizeof(multicastReq));
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
    }
}

bool NetworkReceiverMulticast::receivePacket(std::vector<unsigned char>& data) {
    if (!initialized) return false;

    unsigned char buffer[4096];
    struct sockaddr_in senderAddr {};
#ifdef _WIN32
    int addrLen = sizeof(senderAddr);
#else
    socklen_t addrLen = sizeof(senderAddr);
#endif

    ssize_t bytesRead = recvfrom(sockfd, reinterpret_cast<char*>(buffer), sizeof(buffer), 0, (struct sockaddr*)&senderAddr, &addrLen);
    if (bytesRead < 0) {
        perror("recvfrom failed");
        return false;
    }

    data.assign(buffer, buffer + bytesRead);
    return true;
}
