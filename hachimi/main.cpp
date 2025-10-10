#define NOMINMAX
#define byte win_byte_override

#undef byte
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPlainTextEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QEventLoop>
#include <QCoreApplication>
#include <mysql.h>
#include <iostream>
#include "admin.h"
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
#include "LoginWindow.h"
#include "LogWindow.h"
#include <nlohmann/json.hpp>
#include <fstream>
using nlohmann::json;



int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 启动 Logger（确保先于窗口）
    Logger::instance().info("应用启动");

    // 启动 Server（监听本地端口）
    Server server(8888);
    server.SERstart();
    Logger::instance().info("本地 Server 已启动 (127.0.0.1:8888)");

    // 启动 Client 并连接到本地 Server
    Client client; // 使用默认参数 127.0.0.1:8888
    if (!client.CLTconnectToServer()) {
        Logger::instance().warn("Client 无法连接到 Server，继续启动 UI（部分功能不可用）");
    }
    else {
        Logger::instance().info("Client 已连接到 Server");
    }

    // 显示日志窗口（独立窗口）
    LogWindow* logWin = new LogWindow();
    logWin->setWindowTitle("运行日志");
    logWin->resize(800, 400);
    logWin->show();

    // 准备 DatabaseManager 与 本地用户列表（LoginWindow 现在需要这些参数）
    DatabaseManager* db = new DatabaseManager("127.0.0.1", "root", "a5B3#eF7hJ", "remake",3306); // 根据实际情况替换连接参数
    if (!db->DTBinitialize()) {
        Logger::instance().warn("数据库连接失败，注册/登录功能可能受限");
    }
    std::vector<User> users;
    if (db->DTBisConnected()) {
        users = db->DTBloadAllUsers();
    }

    // 弹出登录窗口（LoginWindow 的构造需要 users 引用和 DatabaseManager*）
    LoginWindow* loginDlg = new LoginWindow(users, db, nullptr);
    // 允许在登录对话框出现时与其它窗口交互（非模态）
    loginDlg->setModal(false);
    loginDlg->setWindowModality(Qt::NonModal);

    // 登录成功后的处理（accepted）
    QObject::connect(loginDlg, &QDialog::accepted, [&app, &client, db, loginDlg]() {
        bool isAdmin = loginDlg->isAdmin();
        QString phone = loginDlg->getPhone();

        if (isAdmin) {
            Logger::instance().info("管理员登录成功，打开管理员窗口");
            AdminWindow* aw = new AdminWindow(&client);
            aw->setWindowTitle("管理员面板");
            aw->resize(1024, 768);
            aw->show();
        } else {
            Logger::instance().info(std::string("用户登录成功，手机号: ") + phone.toStdString());
            UserWindow* uw = nullptr;
            try {
                uw = new UserWindow(phone.toStdString(), &client);
            } catch (...) {
                uw = new UserWindow();
            }
            uw->setWindowTitle("用户面板");
            uw->resize(1024, 768);
            uw->show();
        }

        // 可选择在此删除 loginDlg 或在窗口关闭时 deleteLater
        loginDlg->deleteLater();
    });

    // 用户取消或关闭登录（rejected） -> 退出应用
    QObject::connect(loginDlg, &QDialog::rejected, [db]() {
        Logger::instance().info("用户取消登录，退出应用");
        // 清理 db 后退出主事件循环
        delete db;
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    });

    loginDlg->show();

    // 启动 Qt 事件循环
    int ret = app.exec();

    // 退出流程：优雅断开客户端、停止服务器
    try {
        if (client.CLTisConnectionActive()) client.CLTdisconnect();
    }
    catch (...) {}
    try {
        server.SERstop();
    }
    catch (...) {}

    // 清理 DatabaseManager
    delete db;

    Logger::instance().info("应用退出\n");
    return ret;
}
