#include "NetworkSender.h"

NetworkSender::NetworkSender(const std::string& targetIp, unsigned short targetPort) : initialized(false) {
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

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(targetPort);
#ifdef _WIN32
    serverAddr.sin_addr.S_un.S_addr = inet_addr(targetIp.c_str());
#else
    inet_pton(AF_INET, targetIp.c_str(), &(serverAddr.sin_addr));
#endif
    initialized = true;
}

NetworkSender::~NetworkSender() {
    if (initialized) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
    }
}

bool NetworkSender::sendPacket(const std::vector<unsigned char>& data) {
    if (!initialized) return false;

    // For simplicity, directly send the raw data.
    // In a real system, you'd add RTP headers here or at the encoding stage.
#ifdef _WIN32
    int bytesSent = sendto(sockfd, (const char*)data.data(), static_cast<int>(data.size()), 0,
        (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
        return false;
    }
#else
    ssize_t bytesSent = sendto(sockfd, data.data(), static_cast<int>(data.size()), 0,
        (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (bytesSent < 0) {
        perror("sendto failed");
        return false;
    }
#endif
    return (size_t)bytesSent == data.size();
}