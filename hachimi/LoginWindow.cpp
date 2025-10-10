#include "LoginWindow.h"
#include "databaseManager.h"
#include "admin.h" // 新增：使用 Admin::password
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>

LoginWindow::LoginWindow(std::vector<User>& users, DatabaseManager* db, QWidget* parent)
    : QDialog(parent), usersRef(users), db(db)
{
    // 先询问以管理员还是用户身份启动
    QMessageBox modeBox(this);
    modeBox.setWindowTitle("选择启动模式");
    modeBox.setText("请选择以哪种身份启动：");
    modeBox.setInformativeText("选择 管理员 将用于管理员操作；选择 用户 将进行普通用户登录。");
    QPushButton* adminBtn = modeBox.addButton(tr("管理员"), QMessageBox::AcceptRole);
    QPushButton* userBtn = modeBox.addButton(tr("用户"), QMessageBox::AcceptRole);
    modeBox.addButton(tr("取消"), QMessageBox::RejectRole);
    modeBox.exec();

    QAbstractButton* clicked = modeBox.clickedButton();
    if (clicked == nullptr) {
        // 视为取消
        this->reject();
        return;
    }
    if (clicked == adminBtn) {
        startAsAdmin = true;
    } else if (clicked == userBtn) {
        startAsAdmin = false;
    } else {
        // 取消
        this->reject();
        return;
    }

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

    // 根据选择调整提示（管理员模式下给出提示）
    if (startAsAdmin) {
        infoLabel->setText("管理员模式：请输入管理员密码（手机号可留空或任意填写）。");
        infoLabel->setStyleSheet("color: blue;");
    }

    loginBtn = new QPushButton("登录");
    registerBtn = new QPushButton("注册");

    // 管理员模式禁止注册（隐藏注册按钮），并禁用手机号输入（仅需密码）
    if (startAsAdmin) {
        registerBtn->setEnabled(false);
        registerBtn->setVisible(false);
        phoneEdit->setEnabled(false);
    }

    mainLayout->addLayout(phoneLayout);
    mainLayout->addLayout(pwdLayout);
    mainLayout->addWidget(infoLabel);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(loginBtn);
    btnLayout->addWidget(registerBtn);
    mainLayout->addLayout(btnLayout);

    connect(loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(registerBtn, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
}

QString LoginWindow::getPhone() const {
    return phoneEdit->text();
}

QString LoginWindow::getPassword() const {
    return passwordEdit->text();
}

int LoginWindow::getLoginIndex() const {
    return loginIndex;
}

bool LoginWindow::isAdmin() const {
    return startAsAdmin;
}

QString LoginWindow::userPhone() const {
    return getPhone();
}

void LoginWindow::onLoginClicked() {
    // 管理员模式使用静态硬编码密码验证
    if (startAsAdmin) {
        std::string entered = passwordEdit->text().toStdString();
        if (entered == Admin::password) {
            infoLabel->setText("管理员登录成功！");
            infoLabel->setStyleSheet("color: green;");
            loginIndex = -1; // 管理员不关联普通用户索引
            accept();
        } else {
            infoLabel->setText("管理员密码错误！");
            infoLabel->setStyleSheet("color: red;");
        }
        return;
    }

    // 普通用户登录流程（保持原有逻辑）
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

void LoginWindow::onRegisterClicked() {
    // 管理员模式禁止注册
    if (startAsAdmin) {
        QMessageBox::information(this, "注册", "管理员模式不允许注册。");
        return;
    }

    bool ok;
    QString phone = QInputDialog::getText(this, "注册", "手机号:", QLineEdit::Normal, "", &ok);
    if (!ok || phone.isEmpty()) return;
    QString pwd = QInputDialog::getText(this, "注册", "密码:", QLineEdit::Password, "", &ok);
    if (!ok || pwd.isEmpty()) return;
    QString addr = QInputDialog::getText(this, "注册", "地址:", QLineEdit::Normal, "", &ok);
    if (!ok) return;

    // 先查数据库是否已存在
    User tmpUser("", "", "");
    if (db->DTBloadUser(phone.toStdString(), tmpUser)) {
        QMessageBox::warning(this, "注册", "注册失败，手机号已存在！");
        return;
    }
    // 插入数据库
    if (db->DTBaddUser(User(phone.toStdString(), pwd.toStdString(), addr.toStdString()))) {
        usersRef.push_back(User(phone.toStdString(), pwd.toStdString(), addr.toStdString()));
        QMessageBox::information(this, "注册", "注册成功！");
    } else {
        QMessageBox::warning(this, "注册", "注册失败，数据库错误！");
    }
}