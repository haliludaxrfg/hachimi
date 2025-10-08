#include "Client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

class TCPClient::Impl {
public:
    std::string ip;
    int port;
    SOCKET sock;
    bool connected;

    Impl(const std::string& ip_, int port_)
        : ip(ip_), port(port_), sock(INVALID_SOCKET), connected(false) {}

    ~Impl() {
        if (connected) {
            closesocket(sock);
            WSACleanup();
        }
    }
};

TCPClient::TCPClient(const std::string& ip, int port)
    : pImpl(new Impl(ip, port)) {}

TCPClient::~TCPClient() {
    disconnect();
    delete pImpl;
}

bool TCPClient::connectToServer() {
    std::lock_guard<std::mutex> lock(requestMutex);
    if (pImpl->connected) return true;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    pImpl->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (pImpl->sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(pImpl->port);
    inet_pton(AF_INET, pImpl->ip.c_str(), &serverAddr.sin_addr);

    if (connect(pImpl->sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed\n";
        closesocket(pImpl->sock);
        WSACleanup();
        pImpl->connected = false;
        return false;
    }

    pImpl->connected = true;
    return true;
}

void TCPClient::disconnect() {
    std::lock_guard<std::mutex> lock(requestMutex);
    if (pImpl->connected) {
        closesocket(pImpl->sock);
        WSACleanup();
        pImpl->connected = false;
    }
}

bool TCPClient::isConnectionActive() const {
    return pImpl->connected;
}