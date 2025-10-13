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
#include "PromotionStrategy.h"
#include "Server.h"
#include "TemporaryCart.h"
#include "user.h"
#include "userManager.h"
#include "UserWindow.h"
#include "LoginWindow.h"
#include "LogWindow.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <QTcpSocket>
#include <QHostAddress>
using nlohmann::json;



int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 启动 Logger（确保先于窗口）
    Logger::instance().info("应用启动");

    // —— 改动：尝试探测本地 127.0.0.1:8888 是否已经有 Server 在监听 —— //
    bool serverAlreadyRunning = false;
    {
        QTcpSocket probe;
        probe.connectToHost(QHostAddress("127.0.0.1"), 8888);
        // 等待短时间以判断是否能连接成功（200ms），可根据需要调整
        if (probe.waitForConnected(200)) {
            serverAlreadyRunning = true;
            probe.disconnectFromHost();
        }
    }

    Server* serverPtr = nullptr;
    if (!serverAlreadyRunning) {
        // 没有检测到已有 Server，当前进程启动本地 Server
        serverPtr = new Server(8888);
        serverPtr->SERstart();
        Logger::instance().info("本地 Server 已启动 (127.0.0.1:8888)");
    } else {
        Logger::instance().info("检测到已有本地 Server 监听 127.0.0.1:8888，跳过启动");
    }
    // —— 改动结束 —— //

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

    // 关联 Logger 与 LogWindow（确保日志也显示到 UI）
    Logger* logger = &Logger::instance();
    logger->setLogEdit(logWin->getLogEdit());
    // 明确使用 QueuedConnection 以便跨线程安全地将信号投递到 UI 线程
    QObject::connect(logger, &Logger::logAppended, logWin, &LogWindow::appendLog, Qt::QueuedConnection);
    // 可选：把 std::cout 重定向到 Logger（如果你希望标准输出也出现在 log 窗口）
    logger->redirectCout();

    // 准备 DatabaseManager 与 本地用户列表（LoginWindow 现在需要这些参数）
    DatabaseManager* db = new DatabaseManager("127.0.0.1", "root", "a5B3#eF7hJ", "remake",3306); // 根据实际情况替换连接参数
    if (!db->DTBinitialize()) {
        Logger::instance().warn("数据库连接失败，注册/登录功能可能受限");
    }
    std::vector<User> users;
    if (db->DTBisConnected()) {
        users = db->DTBloadAllUsers();
    }

    // 封装一个函数用于打开登录对话框（可被首次打开和注销后再次调用）
    std::function<void()> openLogin;
    openLogin = [&app, &client, db, &users, &openLogin]() {
        // 在创建 LoginWindow 之前，重新从数据库加载用户列表（若 DB 可用）
        if (db && db->DTBisConnected()) {
            users = db->DTBloadAllUsers();
            Logger::instance().info("已从数据库刷新用户列表用于登录对话框");
        } else {
            Logger::instance().warn("打开登录界面时数据库未连接，使用内存用户列表");
        }

        LoginWindow* loginDlg = new LoginWindow(users, db, &client, nullptr);
        loginDlg->setModal(false);
        loginDlg->setWindowModality(Qt::NonModal);

        // 登录成功后的处理
        QObject::connect(loginDlg, &QDialog::accepted, [loginDlg, &client, db, &users, &openLogin]() {
            bool isAdmin = loginDlg->isAdmin();
            QString phone = loginDlg->getPhone();

            if (isAdmin) {
                Logger::instance().info("管理员登录成功，打开管理员窗口");
                AdminWindow* aw = new AdminWindow(&client);
                aw->setWindowTitle("管理员面板");
                aw->resize(1024, 768);
                aw->show();

                // 点击 AdminWindow 的 “返回身份选择界面” 或触发 backRequested 时
                QObject::connect(aw, &AdminWindow::backRequested, [aw, &openLogin]() {
                    Logger::instance().info("收到 AdminWindow::backRequested，返回登录界面");
                    aw->close();
                    aw->deleteLater();
                    openLogin();
                });

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

                // 注销（删除账号）后重新打开登录界面（已有）
                QObject::connect(uw, &UserWindow::accountDeleted, [uw, &openLogin]() {
                    Logger::instance().info("收到 UserWindow::accountDeleted，重新打开登录窗口");
                    uw->deleteLater();
                    openLogin();
                });

                // 点击 UserWindow 的 “返回身份选择界面” 或触发 backRequested 时
                QObject::connect(uw, &UserWindow::backRequested, [uw, &openLogin]() {
                    Logger::instance().info("收到 UserWindow::backRequested，返回登录界面");
                    uw->close();
                    uw->deleteLater();
                    openLogin();
                });
            }

            // 删除 loginDlg（已接受）
            loginDlg->deleteLater();
        });

        // 用户取消或关闭登录（rejected） -> 退出应用
        QObject::connect(loginDlg, &QDialog::rejected, [db]() {
            Logger::instance().info("用户取消登录，退出应用");
            delete db;
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        });

        loginDlg->show();
    };

    // 首次打开登录窗口
    openLogin();

    // 启动 Qt 事件循环
    int ret = app.exec();

    // 退出流程：优雅断开客户端、停止服务器
    try {
        if (client.CLTisConnectionActive()) client.CLTdisconnect();
    }
    catch (...) {}
    try {
        if (serverPtr) serverPtr->SERstop();
    }
    catch (...) {}

    // 清理 DatabaseManager
    delete db;

    // 若本进程创建了 Server，则释放它
    if (serverPtr) {
        delete serverPtr;
        serverPtr = nullptr;
    }

    Logger::instance().info("应用退出\n");
    return ret;
}
