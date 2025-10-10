#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <vector>
#include "AdminWindow.h"
#include "UserWindow.h"
#include "user.h"
#include "userManager.h"
#include "DatabaseManager.h"

class LoginWindow : public QDialog {
    Q_OBJECT
public:
    LoginWindow(std::vector<User>& users, DatabaseManager* db, QWidget* parent = nullptr);

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

    // 新增：记录启动模式（true = 管理员，false = 普通用户）
    bool startAsAdmin = false;
};
