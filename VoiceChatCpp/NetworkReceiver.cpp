#include "NetworkReceiver.h"

NetworkReceiver::NetworkReceiver(unsigned short listenPort) : listenPort_(listenPort), initialized(false), running(false) {
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
    initialized = true;
}

NetworkReceiver::~NetworkReceiver() {
    stop();
    if (initialized) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
    }
}

bool NetworkReceiver::start() {
    if (!initialized) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    serverAddr.sin_port = htons(listenPort_);

#ifdef _WIN32
    if (bind(sockfd, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(sockfd);
        return false;
    }
#else
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return false;
    }
#endif
    running = true;
    return true;
}

void NetworkReceiver::stop() {
    if (running) {
        running = false;
        // Depending on your OS, you might need to unblock the recvfrom call
        // For example, by sending a dummy packet or closing the socket from another thread.
        // For this simple example, we rely on the loop naturally exiting or the process terminating.
    }
}

std::vector<unsigned char> NetworkReceiver::receivePacketBlocking() {
    if (!running) return {};

    const int MAX_PACKET_SIZE = 4096; // Reasonable max UDP packet size
    std::vector<unsigned char> buffer(MAX_PACKET_SIZE);
    socklen_t addrLen = sizeof(sockaddr_in); // Ensure correct type for recvfrom
    sockaddr_in senderAddr;

#ifdef _WIN32
    int bytesReceived = recvfrom(sockfd, (char*)buffer.data(), static_cast<int>(buffer.size()), 0, (SOCKADDR*)&senderAddr, &addrLen);
    if (bytesReceived == SOCKET_ERROR) {
        if (running) { // Check if we're still supposed to be running
            std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
        }
        return {};
    }
#else
    ssize_t bytesReceived = recvfrom(sockfd, buffer.data(), static_cast<int>(buffer.size()), 0,
        (struct sockaddr*)&senderAddr, &addrLen);
    if (bytesReceived < 0) {
        if (running) {
            perror("recvfrom failed");
        }
        return {};
    }
#endif
    buffer.resize(bytesReceived);
    return buffer;
}