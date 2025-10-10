#include "AdminWindow.h"
#include "logger.h"
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>

// 保留原有注释结构
AdminWindow::AdminWindow(Client* client, QWidget* parent)
    : QWidget(parent), client_(client)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    tabWidget = new QTabWidget(this);

    // 用户管理页
    userTab = new QWidget;
    QVBoxLayout* userLayout = new QVBoxLayout(userTab);
    usersTable = new QTableWidget;
    usersTable->setColumnCount(3);
    usersTable->setHorizontalHeaderLabels(QStringList() << "手机号" << "地址" << "密码");
    usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 新：设置选择模式为整行并禁止单元格编辑
    usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    usersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    userLayout->addWidget(usersTable);

    QHBoxLayout* userBtnLayout = new QHBoxLayout;
    refreshUsersBtn = new QPushButton("刷新用户");
    addUserBtn = new QPushButton("添加用户");       // 新增：创建按钮
    editUserBtn = new QPushButton("修改选中用户");  // 新增：创建按钮
    deleteUserBtn = new QPushButton("删除选中用户");
    userBtnLayout->addWidget(refreshUsersBtn);
    userBtnLayout->addWidget(addUserBtn);
    userBtnLayout->addWidget(editUserBtn);
    userBtnLayout->addWidget(deleteUserBtn);
    userLayout->addLayout(userBtnLayout);

    // 商品管理页
    goodTab = new QWidget;
    QVBoxLayout* goodLayout = new QVBoxLayout(goodTab);
    goodsTable = new QTableWidget;
    goodsTable->setColumnCount(5);
    goodsTable->setHorizontalHeaderLabels(QStringList() << "ID" << "名称" << "价格" << "库存" << "分类");
    goodsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    goodLayout->addWidget(goodsTable);

    QHBoxLayout* goodBtnLayout = new QHBoxLayout;
    refreshGoodsBtn = new QPushButton("刷新商品");
    addGoodBtn = new QPushButton("新增商品");
    editGoodBtn = new QPushButton("编辑选中商品");
    deleteGoodBtn = new QPushButton("删除选中商品");
    goodBtnLayout->addWidget(refreshGoodsBtn);
    goodBtnLayout->addWidget(addGoodBtn);
    goodBtnLayout->addWidget(editGoodBtn);
    goodBtnLayout->addWidget(deleteGoodBtn);
    goodLayout->addLayout(goodBtnLayout);

    // 订单管理页
    orderTab = new QWidget;
    QVBoxLayout* orderLayout = new QVBoxLayout(orderTab);
    ordersTable = new QTableWidget;
    ordersTable->setColumnCount(4);
    ordersTable->setHorizontalHeaderLabels(QStringList() << "订单ID" << "用户手机号" << "状态" << "摘要");
    ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    orderLayout->addWidget(ordersTable);
    refreshOrdersBtn = new QPushButton("刷新订单");
    orderLayout->addWidget(refreshOrdersBtn);

    tabWidget->addTab(userTab, "用户管理");
    tabWidget->addTab(goodTab, "商品管理");
    tabWidget->addTab(orderTab, "订单管理");

    mainLayout->addWidget(tabWidget);

    // 返回按钮
    backBtn = new QPushButton("返回上一级", this);
    connect(backBtn, &QPushButton::clicked, this, &AdminWindow::backRequested);
    mainLayout->addWidget(backBtn);

    // 连接槽（用户部分）
    connect(refreshUsersBtn, &QPushButton::clicked, this, &AdminWindow::refreshUsers);
    connect(addUserBtn, &QPushButton::clicked, this, &AdminWindow::onAddUser);
    connect(editUserBtn, &QPushButton::clicked, this, &AdminWindow::onEditUser);
    connect(deleteUserBtn, &QPushButton::clicked, this, &AdminWindow::onDeleteUser);

    // 连接商品/订单槽（保持已有实现）
    connect(refreshGoodsBtn, &QPushButton::clicked, this, &AdminWindow::refreshGoods);
    connect(addGoodBtn, &QPushButton::clicked, this, &AdminWindow::onAddGood);
    connect(editGoodBtn, &QPushButton::clicked, this, &AdminWindow::onEditGood);
    connect(deleteGoodBtn, &QPushButton::clicked, this, &AdminWindow::onDeleteGood);
    connect(refreshOrdersBtn, &QPushButton::clicked, this, &AdminWindow::refreshOrders);

    // 首次加载
    refreshUsers();
    refreshGoods();
    refreshOrders();
}

void AdminWindow::refreshUsers() {
    usersTable->setRowCount(0);
    if (!client_) return;
    auto users = client_->CLTgetAllAccounts();
    usersTable->setRowCount((int)users.size());
    for (int i = 0; i < (int)users.size(); ++i) {
        const User& u = users[i];
        usersTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(u.getPhone())));
        usersTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(u.getAddress())));
        // 显示明文密码（仅管理员视图），使用访问器获取密码
        usersTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(u.getPassword())));
    }
}

void AdminWindow::onAddUser() {
    Logger::instance().info("AdminWindow::onAddUser called");
    if (!client_) return;
    bool ok;
    QString phone = QInputDialog::getText(this, "新增用户", "手机号:", QLineEdit::Normal, "", &ok);
    if (!ok || phone.isEmpty()) return;
    QString pwd = QInputDialog::getText(this, "新增用户", "密码:", QLineEdit::Password, "", &ok);
    if (!ok || pwd.isEmpty()) return;
    QString addr = QInputDialog::getText(this, "新增用户", "地址:", QLineEdit::Normal, "", &ok);
    if (!ok) return;

    // 注意：Client::CLTaddAccount 在本工程实现为 (phone, password, address)
    bool added = client_->CLTaddAccount(phone.toStdString(), pwd.toStdString(), addr.toStdString());
    Logger::instance().info(std::string("AdminWindow::onAddUser CLTaddAccount returned ") + (added ? "true" : "false"));
    if (!added) {
        QMessageBox::warning(this, "新增用户", "添加账号失败（后台返回错误或无响应）");
        return;
    }
    QMessageBox::information(this, "新增用户", "添加成功");
    refreshUsers();
}

void AdminWindow::onEditUser() {
    Logger::instance().info("AdminWindow::onEditUser called");
    if (!client_) return;
    auto items = usersTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "编辑用户", "请先选择要编辑的用户行");
        return;
    }
    int row = usersTable->row(items.first());
    QString phone = usersTable->item(row, 0)->text();
    QString curAddr = usersTable->item(row, 1)->text();
    QString curPwd = usersTable->item(row, 2)->text();

    bool ok;
    QString newPwd = QInputDialog::getText(this, "编辑用户", "新密码（留空保持不变）:", QLineEdit::Password, curPwd, &ok);
    if (!ok) return;
    if (newPwd.isEmpty()) newPwd = curPwd;

    QString newAddr = QInputDialog::getText(this, "编辑用户", "新地址（留空保持不变）:", QLineEdit::Normal, curAddr, &ok);
    if (!ok) return;
    if (newAddr.isEmpty()) newAddr = curAddr;

    // 若连接断开，尝试重连一次
    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("AdminWindow::onEditUser: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    bool res = client_->CLTupdateUser(phone.toStdString(), newPwd.toStdString(), newAddr.toStdString());
    Logger::instance().info(std::string("AdminWindow::onEditUser CLTupdateUser returned ") + (res ? "true" : "false"));
    if (res) QMessageBox::information(this, "编辑用户", "更新成功");
    else QMessageBox::warning(this, "编辑用户", "更新失败（请检查后台或日志）");
    refreshUsers();
}

void AdminWindow::onDeleteUser() {
    Logger::instance().info("AdminWindow::onDeleteUser called");
    if (!client_) return;
    auto items = usersTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "删除用户", "请先选择要删除的用户行");
        return;
    }
    int row = usersTable->row(items.first());
    QString phone = usersTable->item(row, 0)->text();
    QString pwd = usersTable->item(row, 2)->text(); // 管理员界面显示明文

    if (QMessageBox::question(this, "删除用户", QString("确认删除用户 %1 ?").arg(phone)) != QMessageBox::Yes) return;

    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("AdminWindow::onDeleteUser: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    bool success = client_->CLTdeleteAccount(phone.toStdString(), pwd.toStdString());
    Logger::instance().info(std::string("AdminWindow::onDeleteUser CLTdeleteAccount returned ") + (success ? "true" : "false"));
    QMessageBox::information(this, "删除用户", success ? "删除成功" : "删除失败（请检查后台或日志）");
    refreshUsers();
}

// 以下保持商品与订单实现不变（之前已有实现）
void AdminWindow::refreshGoods() {
    goodsTable->setRowCount(0);
    if (!client_) return;
    auto goods = client_->CLTgetAllGoods();
    goodsTable->setRowCount((int)goods.size());
    for (int i = 0; i < (int)goods.size(); ++i) {
        const Good& g = goods[i];
        goodsTable->setItem(i, 0, new QTableWidgetItem(QString::number(g.getId())));
        goodsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(g.getName())));
        goodsTable->setItem(i, 2, new QTableWidgetItem(QString::number(g.getPrice())));
        goodsTable->setItem(i, 3, new QTableWidgetItem(QString::number(g.getStock())));
        goodsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(g.getCategory())));
    }
}

void AdminWindow::onAddGood() {
    if (!client_) return;
    bool ok;
    QString name = QInputDialog::getText(this, "新增商品", "名称:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;
    double price = QInputDialog::getDouble(this, "新增商品", "价格:", 0.0, 0, 1e9, 2, &ok);
    if (!ok) return;
    int stock = QInputDialog::getInt(this, "新增商品", "库存:", 0, 0, 1e9, 1, &ok);
    if (!ok) return;
    QString cat = QInputDialog::getText(this, "新增商品", "分类:", QLineEdit::Normal, "", &ok);
    if (!ok) return;
    bool res = client_->CLTaddGood(name.toStdString(), price, stock, cat.toStdString());
    QMessageBox::information(this, "新增商品", res ? "新增成功" : "新增失败");
    refreshGoods();
}

void AdminWindow::onEditGood() {
    if (!client_) return;
    auto items = goodsTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "编辑商品", "请先选择要编辑的商品行");
        return;
    }
    int row = goodsTable->row(items.first());
    int id = goodsTable->item(row, 0)->text().toInt();
    QString curName = goodsTable->item(row, 1)->text();
    double curPrice = goodsTable->item(row, 2)->text().toDouble();
    int curStock = goodsTable->item(row, 3)->text().toInt();
    QString curCat = goodsTable->item(row, 4)->text();

    bool ok;
    QString name = QInputDialog::getText(this, "编辑商品", "名称:", QLineEdit::Normal, curName, &ok);
    if (!ok) return;
    double price = QInputDialog::getDouble(this, "编辑商品", "价格:", curPrice, 0, 1e9, 2, &ok);
    if (!ok) return;
    int stock = QInputDialog::getInt(this, "编辑商品", "库存:", curStock, 0, 1e9, 1, &ok);
    if (!ok) return;
    QString cat = QInputDialog::getText(this, "编辑商品", "分类:", QLineEdit::Normal, curCat, &ok);
    if (!ok) return;

    bool res = client_->CLTupdateGood(id, name.toStdString(), price, stock, cat.toStdString());
    QMessageBox::information(this, "编辑商品", res ? "更新成功" : "更新失败");
    refreshGoods();
}

void AdminWindow::onDeleteGood() {
    if (!client_) return;
    auto items = goodsTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "删除商品", "请先选择要删除的商品行");
        return;
    }
    int row = goodsTable->row(items.first());
    int id = goodsTable->item(row, 0)->text().toInt();
    bool res = client_->CLTdeleteGood(id);
    QMessageBox::information(this, "删除商品", res ? "删除成功" : "删除失败");
    refreshGoods();
}

void AdminWindow::refreshOrders() {
    ordersTable->setRowCount(0);
    if (!client_) return;
    auto orders = client_->CLTgetAllOrders(); // 获取所有订单
    ordersTable->setRowCount((int)orders.size());
    for (int i = 0; i < (int)orders.size(); ++i) {
        const Order& o = orders[i];
        ordersTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(o.getOrderId())));
        ordersTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(o.getUserPhone())));
        ordersTable->setItem(i, 2, new QTableWidgetItem(QString::number(o.getStatus())));
        QString summary = QString("%1 件, 总计 %2").arg((int)o.getItems().size()).arg(o.getTotalAmount());
        ordersTable->setItem(i, 3, new QTableWidgetItem(summary));
    }
}