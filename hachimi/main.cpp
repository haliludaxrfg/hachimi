#define NOMINMAX
#define byte win_byte_override

#undef byte
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPlainTextEdit>

#include <mysql.h>
#include <iostream>
//#include "hachimi.h"
//ccbccbccbc4nmb
#include "cartItem.h"
#include "Client.h"
#include "databaseManager.h"
#include "good.h"
#include "logger.h"
#include "order.h"
#include "orderItem.h"
#include "promotion.h"
#include "Server.h"
#include "TemporaryCart.h"
#include "user.h"
#include "userManager.h"
#include "LoginDialog.h"

// 自定义streambuf，将cout输出重定向到QPlainTextEdit
class QtLogStream : public std::streambuf {
public:
    QtLogStream(QPlainTextEdit* edit) : edit(edit) {}
protected:
    virtual int overflow(int c) override {
        if (c != EOF) {
            buffer += static_cast<char>(c);
            if (c == '\n') {
                // 每行输出时用UTF-8解码
                edit->moveCursor(QTextCursor::End);
                edit->insertPlainText(QString::fromUtf8(buffer.c_str()));
                buffer.clear();
            }
        }
        return c;
    }
    virtual int sync() override {
        if (!buffer.empty()) {
            edit->moveCursor(QTextCursor::End);
            edit->insertPlainText(QString::fromUtf8(buffer.c_str()));
            buffer.clear();
        }
        return 0;
    }
private:
    QPlainTextEdit* edit;
    std::string buffer;
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("测试信息窗口");
    QVBoxLayout* layout = new QVBoxLayout(&window);
    QPlainTextEdit* logEdit = new QPlainTextEdit(&window);
    logEdit->setReadOnly(true);
    logEdit->setFont(QFont("方正FW珍珠体 简繁"));
    layout->addWidget(logEdit);
    window.resize(600, 400);
    window.show();

    // 重定向cout
    QtLogStream logStream(logEdit);
    std::streambuf* oldCout = std::cout.rdbuf(&logStream);

    // 1. 测试数据库连接
    DatabaseManager db("localhost", "root", "a5B3#eF7hJ", "remake", 3306);
    if (db.initialize()) {
        std::cout << "数据库连接成功！" << std::endl;
    }
    else {
        std::cout << "数据库连接失败！" << std::endl;
        return app.exec();
    }

    // 2. 启动服务器
    Server server(8888);
    server.start();
    std::cout << "服务器已启动。" << std::endl;

    // 3. 启动客户端并连接服务器
    TCPClient client("127.0.0.1", 8888);
    if (client.connectToServer()) {
        std::cout << "客户端连接服务器成功！" << std::endl;
        // 4. 发送简单请求（假设服务器有processRequest实现）
        std::string response = client.sendRequest("GET_ALL_GOODS");
        std::cout << "服务器响应: " << response << std::endl;
    }
    else {
        std::cout << "客户端连接服务器失败！" << std::endl;
    }

    // 注册和登录测试
    std::vector<User> users;
    // 1. 启动时从数据库加载所有用户
    users = db.loadAllUsers();

    LoginDialog loginDlg(users, &db);
    if (loginDlg.exec() == QDialog::Accepted) {
        std::cout << "用户登录成功，索引: " << loginDlg.getLoginIndex() << std::endl;
        // 可以继续后续操作
    } else {
        std::cout << "用户未登录，程序退出。" << std::endl;
        return 0;
    }

    // 5. 停止服务器
    server.stop();
    std::cout << "服务器已停止。" << std::endl;

    std::cout << "ccbc4nmb" << std::endl;

    // 恢复cout
    std::cout.rdbuf(oldCout);

    return app.exec();
}
/*
int connectTest() {
    // 1. 测试数据库连接
    DatabaseManager db("localhost", "root", "a5B3#eF7hJ", "remake", 3306);
    if (db.initialize()) {
        std::cout << "数据库连接成功！" << std::endl;
    }
    else {
        std::cout << "数据库连接失败！" << std::endl;
        return 1;
    }

    // 2. 启动服务器
    Server server(8888);
    server.start();
    std::cout << "服务器已启动。" << std::endl;

    // 3. 启动客户端并连接服务器
    TCPClient client("127.0.0.1", 8888);
    if (client.connectToServer()) {
        std::cout << "客户端连接服务器成功！" << std::endl;
        // 4. 发送简单请求（假设服务器有processRequest实现）
        std::string response = client.sendRequest("GET_ALL_GOODS");
        std::cout << "服务器响应: " << response << std::endl;
    }
    else {
        std::cout << "客户端连接服务器失败！" << std::endl;
    }

    // 5. 停止服务器
    server.stop();
    std::cout << "服务器已停止。" << std::endl;
    system("pause");
    return 0;
}
*/