#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QObject>
#include "Server.h"
class QtServer : public QTcpServer {
    Q_OBJECT
public:
    QtServer(Server* logic, QObject* parent = nullptr)
        : QTcpServer(parent), logic(logic) {
    }

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        QTcpSocket* clientSocket = new QTcpSocket;
        clientSocket->setSocketDescriptor(socketDescriptor);
        QObject::connect(clientSocket, &QTcpSocket::readyRead, [this, clientSocket]() {
            QByteArray data = clientSocket->readAll();
            std::string req(data.constData(), data.size());
            std::string resp = logic->processRequest(req);
            clientSocket->write(QByteArray::fromStdString(resp));
            });
        QObject::connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
    }
private:
    Server* logic;
};