#include "Client.h"


class TCPClient::Impl {
public:
    QString ip;
    int port;
    QTcpSocket* socket;
    bool connected;

    Impl(const std::string& ip_, int port_)
        : ip(QString::fromStdString(ip_)), port(port_), socket(new QTcpSocket), connected(false) {}

    ~Impl() {
        if (socket) {
            socket->disconnectFromHost();
            socket->waitForDisconnected(1000);
            delete socket;
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
    QMutexLocker lock(&requestMutex);
    if (pImpl->connected) return true;

    pImpl->socket->connectToHost(QHostAddress(pImpl->ip), pImpl->port);
    if (!pImpl->socket->waitForConnected(3000)) {
        qWarning("Connect failed");
        pImpl->connected = false;
        return false;
    }
    pImpl->connected = true;
    return true;
}

void TCPClient::disconnect() {
    QMutexLocker lock(&requestMutex);
    if (pImpl->connected) {
        pImpl->socket->disconnectFromHost();
        pImpl->socket->waitForDisconnected(1000);
        pImpl->connected = false;
    }
}

bool TCPClient::isConnectionActive() const {
    return pImpl->connected;
}

std::string TCPClient::sendRequest(const std::string& request) {
    QMutexLocker lock(&requestMutex);
    if (!pImpl->connected) {
        qWarning("Not connected to server");
        return "";
    }
    QByteArray req = QByteArray::fromStdString(request);
    qint64 sent = pImpl->socket->write(req);
    if (sent == -1) {
        qWarning("Send failed");
        disconnect();
        return "";
    }
    if (!pImpl->socket->waitForReadyRead(3000)) {
        qWarning("No response from server");
        disconnect();
        return "";
    }
    QByteArray resp = pImpl->socket->readAll();
    return resp.toStdString();
}