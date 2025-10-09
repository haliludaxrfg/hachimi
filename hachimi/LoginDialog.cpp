#include "LoginDialog.h"
#include "databaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>

LoginDialog::LoginDialog(std::vector<User>& users, DatabaseManager* db, QWidget* parent)
    : QDialog(parent), usersRef(users), db(db)
{
    setWindowTitle("用户登录");
    setMinimumWidth(300);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* phoneLayout = new QHBoxLayout;
    phoneLayout->addWidget(new QLabel("手机号:"));
    phoneEdit = new QLineEdit;
    phoneLayout->addWidget(phoneEdit);

    QHBoxLayout* pwdLayout = new QHBoxLayout;
    pwdLayout->addWidget(new QLabel("密码:"));
    passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    pwdLayout->addWidget(passwordEdit);

    infoLabel = new QLabel;
    infoLabel->setStyleSheet("color: red;");

    loginBtn = new QPushButton("登录");
    registerBtn = new QPushButton("注册");

    mainLayout->addLayout(phoneLayout);
    mainLayout->addLayout(pwdLayout);
    mainLayout->addWidget(infoLabel);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(loginBtn);
    btnLayout->addWidget(registerBtn);
    mainLayout->addLayout(btnLayout);

    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(registerBtn, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
}

QString LoginDialog::getPhone() const {
    return phoneEdit->text();
}

QString LoginDialog::getPassword() const {
    return passwordEdit->text();
}

int LoginDialog::getLoginIndex() const {
    return loginIndex;
}

void LoginDialog::onLoginClicked() {
    std::string phone = phoneEdit->text().toStdString();
    std::string pwd = passwordEdit->text().toStdString();
    int idx = UserManager::loginUser(usersRef, phone, pwd);
    if (idx >= 0) {
        infoLabel->setText("登录成功！");
        infoLabel->setStyleSheet("color: green;");
        loginIndex = idx;
        accept();
    }
    else {
        infoLabel->setText("手机号或密码错误！");
        infoLabel->setStyleSheet("color: red;");
    }
}

void LoginDialog::onRegisterClicked() {
    bool ok;
    QString phone = QInputDialog::getText(this, "注册", "手机号:", QLineEdit::Normal, "", &ok);
    if (!ok || phone.isEmpty()) return;
    QString pwd = QInputDialog::getText(this, "注册", "密码:", QLineEdit::Password, "", &ok);
    if (!ok || pwd.isEmpty()) return;
    QString addr = QInputDialog::getText(this, "注册", "地址:", QLineEdit::Normal, "", &ok);
    if (!ok) return;

    // 先查数据库是否已存在
    User tmpUser("", "", "");
    if (db->loadUser(phone.toStdString(), tmpUser)) {
        QMessageBox::warning(this, "注册", "注册失败，手机号已存在！");
        return;
    }
    // 插入数据库
    if (db->addUser(User(phone.toStdString(), pwd.toStdString(), addr.toStdString()))) {
        usersRef.push_back(User(phone.toStdString(), pwd.toStdString(), addr.toStdString()));
        QMessageBox::information(this, "注册", "注册成功！");
    } else {
        QMessageBox::warning(this, "注册", "注册失败，数据库错误！");
    }
}