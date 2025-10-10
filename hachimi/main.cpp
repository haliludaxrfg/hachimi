#define NOMINMAX
#define byte win_byte_override

#undef byte
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPlainTextEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <mysql.h>
#include <iostream>
//#include "hachimi.h"
//ccbccbccbc4nmb
#include "admin.h"
#include "AdminWindow.h"
#include "AdminWindow.h"
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
#include "UserWindow.h"
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
    if (db.DTBinitialize()) {
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
        std::string response = client.sendRequest("GET_ALL_GOODS");
        std::cout << "服务器响应: " << response << std::endl;
    }
    else {
        std::cout << "客户端连接服务器失败！" << std::endl;
    }

    // ===== 新增：身份选择 =====
    QStringList roles;
    roles << "管理员" << "用户";
    bool ok = false;
    QString role = QInputDialog::getItem(nullptr, "选择身份", "请选择登录身份：", roles, 0, false, &ok);
    if (!ok) return 0;

    if (role == "管理员") {
        bool ok = false;
        QString inputPwd = QInputDialog::getText(
            nullptr, "管理员登录", "请输入管理员密码：", QLineEdit::Password, "", &ok);
        if (!ok) return 0;
        if (inputPwd.toStdString() != Admin::password) {
            QMessageBox::warning(nullptr, "登录失败", "管理员密码错误！");
            return 0;
        }
        AdminWindow* adminWin = new AdminWindow();
        adminWin->show();
    } else {
        // 用户注册和登录
        std::vector<User> users;
        users = db.DTBloadAllUsers();

        LoginDialog loginDlg(users, &db);
        if (loginDlg.exec() == QDialog::Accepted) {
            std::cout << "用户登录成功，索引: " << loginDlg.getLoginIndex() << std::endl;
            UserWindow* userWin = new UserWindow();
            userWin->show();
        } else {
            std::cout << "用户未登录，程序退出。" << std::endl;
            return 0;
        }
    }

    // 5. 停止服务器
    server.stop();
    std::cout << "服务器已停止。" << std::endl;

    std::cout << "ccbc4nmb" << std::endl;

    // 恢复cout
    std::cout.rdbuf(oldCout);

    return app.exec();
}
