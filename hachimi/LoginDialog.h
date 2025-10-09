#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <vector>
#include "user.h"
#include "userManager.h"
#include "DatabaseManager.h"

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    LoginDialog(std::vector<User>& users, DatabaseManager* db, QWidget* parent = nullptr);

    QString getPhone() const;
    QString getPassword() const;
    int getLoginIndex() const;

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
};
