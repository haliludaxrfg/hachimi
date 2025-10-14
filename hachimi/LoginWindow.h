#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <vector>
#include "user.h"
#include "userManager.h"
#include "DatabaseManager.h"
#include <functional>
#include <QElapsedTimer> // 新增：节流计时器

// 前向声明，避免头文件循环包含
class Client;
class AdminWindow;
class UserWindow;

class LoginWindow : public QDialog {
    Q_OBJECT
public:
    // 增加 Client* 参数（可为空）
    LoginWindow(std::vector<User>& users, DatabaseManager* db, Client* client = nullptr, QWidget* parent = nullptr);

    QString getPhone() const;
    QString getPassword() const;
    int getLoginIndex() const;

    // 新增：外部可查询用户选择（管理员/用户）以及用户手机号
    bool isAdmin() const;
    QString userPhone() const;

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    QLineEdit* phoneEdit;
    QLineEdit* passwordEdit;
    QLabel* infoLabel;
    QPushButton* loginBtn;
    QPushButton* registerBtn;
    int loginIndex = -1;
    std::vector<User>& usersRef;
    DatabaseManager* db;

    // 新增：Client 指针（可空）
    Client* client_;

    // 新增：记录启动模式（true = 管理员，false = 普通用户）
    bool startAsAdmin = false;

    // 每秒操作限制相关（LoginWindow 同样支持节流）
    QElapsedTimer actionTimer_;
    bool tryThrottle(QWidget* parent = nullptr);
};

