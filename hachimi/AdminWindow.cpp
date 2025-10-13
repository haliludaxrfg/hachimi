#include "AdminWindow.h"
#include "logger.h"
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <nlohmann/json.hpp>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>

// 状态映射辅助函数
static QString orderStatusToText(int status) {
    switch (status) {
    case 1: return QStringLiteral("已完成");
    case 2: return QStringLiteral("已发货");
    case 3: return QStringLiteral("已退货");
    case 4: return QStringLiteral("维修中");
    case 5: return QStringLiteral("已取消");
    case 0: return QStringLiteral("未知");
    default: return QString::number(status);
    }
}

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
    ordersTable->setColumnCount(5);
    ordersTable->setHorizontalHeaderLabels(QStringList() << "订单ID" << "用户手机号" << "地址" << "状态" << "摘要");
    ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    orderLayout->addWidget(ordersTable);

    // 订单按钮：刷新 + 发起退货/维修/删除/查看详情
    QHBoxLayout* orderBtnLayout = new QHBoxLayout;
    refreshOrdersBtn = new QPushButton("刷新订单");
    viewOrderBtn   = new QPushButton("查看详情");
    returnOrderBtn  = new QPushButton("发起退货");
    repairOrderBtn  = new QPushButton("发起维修");
    deleteOrderBtn  = new QPushButton("删除订单");
    orderBtnLayout->addWidget(refreshOrdersBtn);
    orderBtnLayout->addWidget(viewOrderBtn);
    orderBtnLayout->addWidget(returnOrderBtn);
    orderBtnLayout->addWidget(repairOrderBtn);
    orderBtnLayout->addWidget(deleteOrderBtn);
    orderLayout->addLayout(orderBtnLayout);

    // 购物车管理页（移除 添加/保存，保留加载/修改/删除）
    cartTab = new QWidget;
    QVBoxLayout* cartLayout = new QVBoxLayout(cartTab);
    QHBoxLayout* cartTop = new QHBoxLayout;
    cartPhoneEdit = new QLineEdit;
    cartPhoneEdit->setPlaceholderText("输入用户手机号");
    loadCartBtn = new QPushButton("加载购物车");
    cartTop->addWidget(new QLabel("用户手机号:"));
    cartTop->addWidget(cartPhoneEdit);
    cartTop->addWidget(loadCartBtn);
    cartLayout->addLayout(cartTop);

    cartTable = new QTableWidget;
    cartTable->setColumnCount(5);
    cartTable->setHorizontalHeaderLabels(QStringList() << "good_id" << "名称" << "价格" << "数量" << "小计");
    cartTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable->setSelectionMode(QAbstractItemView::SingleSelection);
    cartTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cartLayout->addWidget(cartTable);

    QHBoxLayout* cartBtnLayout = new QHBoxLayout;
    editCartItemBtn = new QPushButton("修改数量");
    removeCartItemBtn = new QPushButton("删除条目");
    cartBtnLayout->addWidget(editCartItemBtn);
    cartBtnLayout->addWidget(removeCartItemBtn);
    cartLayout->addLayout(cartBtnLayout);

    tabWidget->addTab(userTab, "用户管理");
    tabWidget->addTab(goodTab, "商品管理");
    tabWidget->addTab(orderTab, "订单管理");
    tabWidget->addTab(cartTab, "购物车管理");

    mainLayout->addWidget(tabWidget);

    // 返回按钮与“返回身份选择界面”并列
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout;
    backBtn = new QPushButton("返回上一级", this);
    returnIdentityBtn = new QPushButton("返回身份选择界面", this);
    bottomBtnLayout->addStretch();
    bottomBtnLayout->addWidget(backBtn);
    bottomBtnLayout->addWidget(returnIdentityBtn);
    mainLayout->addLayout(bottomBtnLayout);

    connect(backBtn, &QPushButton::clicked, this, &AdminWindow::backRequested);
    connect(returnIdentityBtn, &QPushButton::clicked, this, &AdminWindow::onReturnToIdentitySelection);

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

    // 连接订单操作按钮
    connect(viewOrderBtn, &QPushButton::clicked, this, &AdminWindow::onViewOrderDetail);
    connect(returnOrderBtn, &QPushButton::clicked, this, &AdminWindow::onReturnOrder);
    connect(repairOrderBtn, &QPushButton::clicked, this, &AdminWindow::onRepairOrder);
    connect(deleteOrderBtn, &QPushButton::clicked, this, &AdminWindow::onDeleteOrder);

    // 购物车槽（移除添加/保存连接）
    connect(loadCartBtn, &QPushButton::clicked, this, &AdminWindow::onLoadCartForUser);
    connect(editCartItemBtn, &QPushButton::clicked, this, &AdminWindow::onEditCartItem);
    connect(removeCartItemBtn, &QPushButton::clicked, this, &AdminWindow::onRemoveCartItem);

    // 创建促销标签页
    createPromotionsTab();

    // 首次加载
    refreshUsers();
    refreshGoods();
    refreshOrders();
    refreshPromotions();
}

// ---------------- 用户/商品/订单 已在之前文件中实现 ----------------
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

void AdminWindow::onSearchUser() {
    Logger::instance().info("AdminWindow::onSearchUser called");
    if (!client_) return;
    bool ok;
    QString phone = QInputDialog::getText(this, "查询用户", "手机号:", QLineEdit::Normal, "", &ok);
    if (!ok) return;
    phone = phone.trimmed();
    if (phone.isEmpty()) {
        QMessageBox::warning(this, "查询用户", "请输入手机号");
        return;
    }

    // 获取所有用户并在客户端筛选（服务端暂无按手机号单独查询的 API）
    auto users = client_->CLTgetAllAccounts();
    for (int i = 0; i < (int)users.size(); ++i) {
        if (QString::fromStdString(users[i].getPhone()) == phone) {
            // 找到：只显示这一行
            usersTable->setRowCount(1);
            const User& u = users[i];
            usersTable->setItem(0, 0, new QTableWidgetItem(QString::fromStdString(u.getPhone())));
            usersTable->setItem(0, 1, new QTableWidgetItem(QString::fromStdString(u.getAddress())));
            usersTable->setItem(0, 2, new QTableWidgetItem(QString::fromStdString(u.getPassword())));
            QMessageBox::information(this, "查询用户", "找到匹配用户，表格已过滤显示该用户。若要查看全部用户，请点击“刷新用户”。");
            return;
        }
    }

    QMessageBox::information(this, "查询用户", "未找到匹配的用户");
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
    QString pwd = usersTable->item(row, 2)->text(); // 管理員界面显示明文

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

// 替换 AdminWindow::refreshOrders()：在表格中显示地址（优先使用 Order 中的地址，否则拉取明细）
void AdminWindow::refreshOrders() {
    ordersTable->setRowCount(0);
    if (!client_) return;
    auto orders = client_->CLTgetAllOrders(); // 获取所有订单
    ordersTable->setRowCount((int)orders.size());
    for (int i = 0; i < (int)orders.size(); ++i) {
        const Order& o = orders[i];
        ordersTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(o.getOrderId())));
        ordersTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(o.getUserPhone())));

        // 填地址：优先 Order 自带，否则尝试拉取详情
        QString addr = QString::fromStdString(o.getShippingAddress());
        if (addr.isEmpty()) {
            Order detail;
            bool ok = client_->CLTgetOrderDetail(o.getOrderId(), o.getUserPhone(), detail);
            if (ok) addr = QString::fromStdString(detail.getShippingAddress());
        }
        ordersTable->setItem(i, 2, new QTableWidgetItem(addr));

        ordersTable->setItem(i, 3, new QTableWidgetItem(orderStatusToText(o.getStatus())));
        QString summary = QString("总计 %1").arg(o.getTotalAmount());
        ordersTable->setItem(i, 4, new QTableWidgetItem(summary));
    }
}

// --------------- 新增：订单操作实现 ---------------
void AdminWindow::onReturnOrder() {
    if (!client_) return;
    auto items = ordersTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "发起退货", "请先选择要退货的订单行"); return; }
    int row = ordersTable->row(items.first());
    QString orderId = ordersTable->item(row, 0)->text();
    QString userPhone = ordersTable->item(row, 1) ? ordersTable->item(row,1)->text() : QString();

    if (QMessageBox::question(this, "发起退货", QString("确认对订单 %1 发起退货？").arg(orderId)) != QMessageBox::Yes) return;

    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    bool ok = client_->CLTreturnSettledOrder(orderId.toStdString(), userPhone.toStdString());
    Logger::instance().info(std::string("AdminWindow::onReturnOrder CLTreturnSettledOrder returned ") + (ok ? "true" : "false"));
    QMessageBox::information(this, "发起退货", ok ? "退货请求已发出" : "退货失败（查看日志）");
    // 刷新订单状态/列表
    refreshOrders();
}

void AdminWindow::onRepairOrder() {
    if (!client_) return;
    auto items = ordersTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "发起维修", "请先选择要维修的订单行"); return; }
    int row = ordersTable->row(items.first());
    QString orderId = ordersTable->item(row, 0)->text();
    QString userPhone = ordersTable->item(row, 1) ? ordersTable->item(row,1)->text() : QString();

    if (QMessageBox::question(this, "发起维修", QString("确认对订单 %1 发起维修？").arg(orderId)) != QMessageBox::Yes) return;

    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    bool ok = client_->CLTrepairSettledOrder(orderId.toStdString(), userPhone.toStdString());
    Logger::instance().info(std::string("AdminWindow::onRepairOrder CLTrepairSettledOrder returned ") + (ok ? "true" : "false"));
    QMessageBox::information(this, "发起维修", ok ? "维修请求已发出" : "维修请求失败（查看日志）");
    refreshOrders();
}

void AdminWindow::onDeleteOrder() {
    if (!client_) return;
    auto items = ordersTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "删除订单", "请先选择要删除的订单行"); return; }
    int row = ordersTable->row(items.first());
    QString orderId = ordersTable->item(row, 0)->text();
    QString userPhone = ordersTable->item(row, 1) ? ordersTable->item(row,1)->text() : QString();

    if (QMessageBox::question(this, "删除订单", QString("确认删除订单 %1 ? 注意：此操作不可撤销。").arg(orderId)) != QMessageBox::Yes) return;

    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    bool ok = client_->CLTdeleteSettledOrder(orderId.toStdString(), userPhone.toStdString());
    Logger::instance().info(std::string("AdminWindow::onDeleteOrder CLTdeleteSettledOrder returned ") + (ok ? "true" : "false"));
    QMessageBox::information(this, "删除订单", ok ? "删除成功" : "删除失败（查看日志）");
    refreshOrders();
}
// --------------------------------------------------

// ---------------- 购物车相关实现（已移除 增/保存） ----------------
void AdminWindow::onLoadCartForUser() {
    if (!client_) return;
    QString phone = cartPhoneEdit->text().trimmed();
    if (phone.isEmpty()) {
        QMessageBox::warning(this, "加载购物车", "请输入手机号");
        return;
    }
    Logger::instance().info(std::string("AdminWindow::onLoadCartForUser: request for userPhone=") + phone.toStdString());
    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("AdminWindow::onLoadCartForUser: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }
    TemporaryCart cart = client_->CLTgetCartForUser(phone.toStdString());
    currentCart_ = cart; // cache
    cartTable->setRowCount(0);
    if (cart.items.empty() && cart.cart_id.empty()) {
        QMessageBox::information(this, "加载购物车", "未找到购物车或购物车为空");
        return;
    }
    cartTable->setRowCount((int)cart.items.size());
    for (int i = 0; i < (int)cart.items.size(); ++i) {
        const CartItem& it = cart.items[i];
        cartTable->setItem(i, 0, new QTableWidgetItem(QString::number(it.good_id)));
        cartTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(it.good_name)));
        cartTable->setItem(i, 2, new QTableWidgetItem(QString::number(it.price)));
        cartTable->setItem(i, 3, new QTableWidgetItem(QString::number(it.quantity)));
        cartTable->setItem(i, 4, new QTableWidgetItem(QString::number(it.subtotal)));
    }
    Logger::instance().info(std::string("AdminWindow::onLoadCartForUser: parsed cart_id=") + cart.cart_id + ", items=" + std::to_string(cart.items.size()));
}

void AdminWindow::onEditCartItem() {
    if (!client_) return;
    QString phone = cartPhoneEdit->text().trimmed();
    if (phone.isEmpty()) { QMessageBox::warning(this, "修改数量", "请输入手机号后再修改"); return; }
    auto items = cartTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "修改数量", "请先选择要修改的商品行"); return; }
    int row = cartTable->row(items.first());
    int productId = cartTable->item(row, 0)->text().toInt();
    int curQty = cartTable->item(row, 3)->text().toInt();

    bool ok;
    int newQty = QInputDialog::getInt(this, "修改数量", "新数量:", curQty, 0, 1e9, 1, &ok);
    if (!ok) return;

    if (!client_->CLTisConnectionActive()) { client_->CLTreconnect(); }

    // 若 newQty == 0，建议删除
    if (newQty == 0) {
        if (QMessageBox::question(this, "删除条目", "输入数量为 0，是否删除该条目？") == QMessageBox::Yes) {
            bool rem = client_->CLTremoveFromCart(phone.toStdString(), productId);
            Logger::instance().info(std::string("AdminWindow::onEditCartItem -> remove CLTremoveFromCart returned ") + (rem ? "true" : "false"));
            if (!rem) { QMessageBox::warning(this, "删除条目", "删除失败"); return; }
            QMessageBox::information(this, "删除条目", "删除成功，正在刷新购物车");
            onLoadCartForUser();
            return;
        } else {
            return;
        }
    }

    bool res = client_->CLTupdateCartItem(phone.toStdString(), productId, newQty);
    Logger::instance().info(std::string("AdminWindow::onEditCartItem CLTupdateCartItem returned ") + (res ? "true" : "false"));
    if (!res) { QMessageBox::warning(this, "修改数量", "更新失败（检查日志）"); return; }
    QMessageBox::information(this, "修改数量", "更新成功，正在刷新购物车");
    onLoadCartForUser();
}

void AdminWindow::onRemoveCartItem() {
    if (!client_) return;
    QString phone = cartPhoneEdit->text().trimmed();
    if (phone.isEmpty()) { QMessageBox::warning(this, "删除条目", "请输入手机号后再删除"); return; }
    auto items = cartTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "删除条目", "请先选择要删除的商品行"); return; }
    int row = cartTable->row(items.first());
    int productId = cartTable->item(row, 0)->text().toInt();

    if (QMessageBox::question(this, "删除条目", QString("确认从 %1 的购物车删除商品 ID=%2 ?").arg(phone).arg(productId)) != QMessageBox::Yes) return;

    if (!client_->CLTisConnectionActive()) { client_->CLTreconnect(); }
    bool res = client_->CLTremoveFromCart(phone.toStdString(), productId);
    Logger::instance().info(std::string("AdminWindow::onRemoveCartItem CLTremoveFromCart returned ") + (res ? "true" : "false"));
    if (!res) { QMessageBox::warning(this, "删除条目", "删除失败（检查日志）"); return; }
    QMessageBox::information(this, "删除条目", "删除成功，正在刷新购物车");
    onLoadCartForUser();
}

// 新增：返回身份选择界面槽实现
void AdminWindow::onReturnToIdentitySelection() {
    Logger::instance().info("AdminWindow::onReturnToIdentitySelection called");
    emit backRequested();
    this->close();
}

// 管理员查看订单详情实现（调用 Client 获取远端详情）
void AdminWindow::onViewOrderDetail() {
    if (!client_) return;
    auto items = ordersTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "查看订单", "请先选择订单行"); return; }
    int row = ordersTable->row(items.first());
    QString oid = ordersTable->item(row, 0)->text();
    QString userPhone = ordersTable->item(row, 1) ? ordersTable->item(row,1)->text() : QString();

    Order detail;
    bool ok = client_->CLTgetOrderDetail(oid.toStdString(), userPhone.toStdString(), detail);
    if (!ok) { QMessageBox::warning(this, "查看订单", "获取订单详情失败"); return; }

    // 计算基于明细项的原价总额，作为回退值
    double computedTotal = 0.0;
    for (const auto& it : detail.getItems()) {
        computedTotal += it.getPrice() * it.getQuantity();
    }
    // 若服务器返回的 total_amount 有意义（> 0.005），优先使用；否则使用计算值
    double totalToShow = detail.getTotalAmount();
    if (totalToShow <= 0.005) totalToShow = computedTotal;

    // 构造显示文本（金额格式保留两位小数）
    QString txt;
    txt += "订单ID: " + QString::fromStdString(detail.getOrderId()) + "\n";
    txt += "手机号: " + QString::fromStdString(detail.getUserPhone()) + "\n";
    txt += "地址: " + QString::fromStdString(detail.getShippingAddress()) + "\n";
    txt += "状态: " + orderStatusToText(detail.getStatus()) + "\n";
    txt += "总额: " + QString::number(totalToShow, 'f', 2) + "  实付: " + QString::number(detail.getFinalAmount(), 'f', 2) + "\n\n";
    txt += "订单项:\n";
    const auto& itemsVec = detail.getItems();
    if (itemsVec.empty()) {
        txt += "(无订单项)\n";
    } else {
        for (const auto& it : itemsVec) {
            txt += QString::fromStdString(it.getGoodName()) + " (id:" + QString::number(it.getGoodId()) + ") x" + QString::number(it.getQuantity())
                   + " 单价:" + QString::number(it.getPrice(), 'f', 2) + " 小计:" + QString::number(it.getSubtotal(), 'f', 2) + "\n";
        }
    }
    QMessageBox::information(this, "订单详情", txt);
}

// ------------------- 促销相关实现 -------------------
// 在构造函数中创建 promotions tab（示例放在 AdminWindow 构造尾部或合适位置）
void AdminWindow::createPromotionsTab() {
    promoTab = new QWidget(this);
    QVBoxLayout* v = new QVBoxLayout(promoTab);

    promoTable = new QTableWidget(0, 4, promoTab);
    QStringList headers;
    headers << "id" << "name" << "type" << "policy";
    promoTable->setHorizontalHeaderLabels(headers);
    promoTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    promoTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    promoTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    promoTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    promoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    promoTable->setSelectionMode(QAbstractItemView::SingleSelection);

    QHBoxLayout* btnRow = new QHBoxLayout;
    refreshPromosBtn = new QPushButton("刷新", promoTab);

    addPromoBtn = new QPushButton("新增", promoTab);

    deletePromoBtn = new QPushButton("删除", promoTab);
    btnRow->addWidget(refreshPromosBtn);
    btnRow->addWidget(addPromoBtn);

    btnRow->addWidget(deletePromoBtn);
    btnRow->addStretch();

    v->addLayout(btnRow);
    v->addWidget(promoTable);
    tabWidget->addTab(promoTab, "促销");

    // 连接信号：刷新、删除、以及新增和双击编辑
    connect(refreshPromosBtn, &QPushButton::clicked, this, &AdminWindow::refreshPromotions);
    connect(deletePromoBtn, &QPushButton::clicked, this, &AdminWindow::onDeletePromotion);

    // 新增：连接新增按钮到实现好的 onAddPromotion()
    connect(addPromoBtn, &QPushButton::clicked, this, &AdminWindow::onAddPromotion);

    // 方便：双击表格行进入编辑（使用已有 onEditPromotion）
    connect(promoTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        Q_UNUSED(row);
        this->onEditPromotion();
        });
}

// helper: show simple editor dialog for add/edit
static bool showPromotionEditor(QWidget* parent, const std::string& inName, const std::string& inType, const std::string& inPolicy, std::string& outName, std::string& outType, std::string& outPolicy) {
    QDialog dlg(parent);
    dlg.setWindowTitle(inName.empty() ? "新增促销" : "编辑促销");
    QVBoxLayout* v = new QVBoxLayout(&dlg);

    QHBoxLayout* row1 = new QHBoxLayout;
    row1->addWidget(new QLabel("名称:"));
    QLineEdit* nameEdit = new QLineEdit(QString::fromStdString(inName));
    row1->addWidget(nameEdit);
    v->addLayout(row1);

    QHBoxLayout* row2 = new QHBoxLayout;
    row2->addWidget(new QLabel("类型:"));
    QLineEdit* typeEdit = new QLineEdit(QString::fromStdString(inType));
    row2->addWidget(typeEdit);
    v->addLayout(row2);

    v->addWidget(new QLabel("policy JSON 或 policy_detail 字符串:"));
    QTextEdit* policyEdit = new QTextEdit(QString::fromStdString(inPolicy));
    policyEdit->setMinimumHeight(160);
    v->addWidget(policyEdit);

    QHBoxLayout* br = new QHBoxLayout;
    QPushButton* ok = new QPushButton("确定");
    QPushButton* cancel = new QPushButton("取消");
    br->addStretch();
    br->addWidget(ok);
    br->addWidget(cancel);
    v->addLayout(br);

    // 使用 QObject::connect 避免与 winsock 的 connect 冲突
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return false;
    outName = nameEdit->text().toStdString();
    outType = typeEdit->text().toStdString();
    outPolicy = policyEdit->toPlainText().toStdString();
    return true;
}

// 显示选择促销类型
bool AdminWindow::showPromotionTypeSelector(QWidget* parent, PromotionKind& outKind) {
    QStringList items;
    items << "统一折扣（X 折）" << "阶梯折扣（多档折扣）" << "满 X 减 Y";
    bool ok;
    QString choice = QInputDialog::getItem(parent, "选择促销类型", "促销类型:", items, 0, false, &ok);
    if (!ok) return false;
    if (choice == items[0]) outKind = PromotionKind::Discount;
    else if (choice == items[1]) outKind = PromotionKind::Tiered;
    else if (choice == items[2]) outKind = PromotionKind::FullReduction;
    else outKind = PromotionKind::Unknown;
    return true;
}

// 替换原有的 showTypedPromotionEditor 实现，新增折扣值校验（确保折扣 <= 1）
bool AdminWindow::showTypedPromotionEditor(QWidget* parent, PromotionKind kind,
                                           const std::string& inName, const nlohmann::json& inPolicyJson, const std::string& inPolicyDetail,
                                           std::string& outName, nlohmann::json& outPolicyJson, std::string& outPolicyDetail) {
    QDialog dlg(parent);
    dlg.setWindowTitle(inName.empty() ? "新增促销" : "编辑促销");
    QVBoxLayout* main = new QVBoxLayout(&dlg);
    QFormLayout* form = new QFormLayout;
    QLineEdit* nameEdit = new QLineEdit(QString::fromStdString(inName));
    form->addRow(new QLabel("名称:"), nameEdit);

    if (kind == PromotionKind::Discount) {
        QDoubleSpinBox* rate = new QDoubleSpinBox;
        // 限制折扣范围为 (0.01 .. 1.00)，确保折后不大于原价
        rate->setRange(0.01, 1.0);
        rate->setDecimals(4);
        rate->setSingleStep(0.01);
        if (inPolicyJson.contains("discount")) rate->setValue(inPolicyJson.value("discount", 1.0));
        else if (!inPolicyDetail.empty()) {
            try { auto p = nlohmann::json::parse(inPolicyDetail); if (p.contains("discount")) rate->setValue(p.value("discount", 1.0)); } catch(...) {}
        }
        form->addRow(new QLabel("折扣率（例如 0.9 表示 9 折，必须 <= 1）:"), rate);

        main->addLayout(form);
        QHBoxLayout* hb = new QHBoxLayout;
        QPushButton* ok = new QPushButton("确定");
        QPushButton* cancel = new QPushButton("取消");
        hb->addStretch(); hb->addWidget(ok); hb->addWidget(cancel);
        main->addLayout(hb);
        QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
        QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted) return false;

        // 再次校验（以防程序逻辑修改了范围）
        double v = rate->value();
        if (v > 1.0) {
            QMessageBox::warning(parent, "折扣错误", "折扣率必须小于或等于 1（不能超过原价）。");
            return false;
        }

        outName = nameEdit->text().toStdString();
        outPolicyJson = nlohmann::json::object();
        outPolicyJson["type"] = "discount";
        outPolicyJson["discount"] = v;
        outPolicyJson["scope"] = "global";
        outPolicyDetail.clear();
        return true;
    }
    else if (kind == PromotionKind::Tiered) {
        // 固定 3 个阶梯输入：每行一个 min_qty 与 discount（折扣率）
        QWidget* tiersWidget = new QWidget;
        QFormLayout* tiersLayout = new QFormLayout(tiersWidget);

        QSpinBox* minQtyBoxes[3];
        QDoubleSpinBox* discountBoxes[3];
        for (int t = 0; t < 3; ++t) {
            minQtyBoxes[t] = new QSpinBox;
            minQtyBoxes[t]->setRange(0, 1000000);
            minQtyBoxes[t]->setValue(0);
            discountBoxes[t] = new QDoubleSpinBox;
            // 限制每档折扣最大为 1.0，避免出现大于 1 的“加价”折扣
            discountBoxes[t]->setRange(0.0, 1.0);
            discountBoxes[t]->setDecimals(4);
            discountBoxes[t]->setSingleStep(0.01);
            discountBoxes[t]->setValue(1.0); // 默认不打折

            QHBoxLayout* row = new QHBoxLayout;
            row->addWidget(new QLabel(QString("第%1阶 阈值(min_qty):").arg(t+1)));
            row->addWidget(minQtyBoxes[t]);
            row->addSpacing(8);
            row->addWidget(new QLabel("折扣(例如0.9表示9折，必须 <=1):"));
            row->addWidget(discountBoxes[t]);

            tiersLayout->addRow(row);
        }

        // 预填 tiers（如果有）
        try {
            if (inPolicyJson.contains("tiers") && inPolicyJson["tiers"].is_array()) {
                auto arr = inPolicyJson["tiers"];
                for (int t = 0; t < 3 && t < (int)arr.size(); ++t) {
                    try {
                        if (arr[t].contains("min_qty")) minQtyBoxes[t]->setValue(arr[t].value("min_qty", 0));
                        if (arr[t].contains("discount")) discountBoxes[t]->setValue(arr[t].value("discount", 1.0));
                    } catch (...) {}
                }
            } else if (!inPolicyDetail.empty()) {
                try {
                    auto parsed = nlohmann::json::parse(inPolicyDetail);
                    if (parsed.contains("tiers") && parsed["tiers"].is_array()) {
                        auto arr = parsed["tiers"];
                        for (int t = 0; t < 3 && t < (int)arr.size(); ++t) {
                            try {
                                if (arr[t].contains("min_qty")) minQtyBoxes[t]->setValue(arr[t].value("min_qty", 0));
                                if (arr[t].contains("discount")) discountBoxes[t]->setValue(arr[t].value("discount", 1.0));
                            } catch (...) {}
                        }
                    }
                } catch (...) {}
            }
        } catch (...) {}

        form->addRow(new QLabel("三个阶梯（阈值 = 商品总数量；折扣 = 单项折扣率，均必须 <=1）"), tiersWidget);

        main->addLayout(form);
        QHBoxLayout* hb = new QHBoxLayout;
        QPushButton* ok = new QPushButton("确定");
        QPushButton* cancel = new QPushButton("取消");
        hb->addStretch(); hb->addWidget(ok); hb->addWidget(cancel);
        main->addLayout(hb);
        QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
        QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted) {
            return false;
        }

        // 验证：三个阶梯的 min_qty 必须严格递增
        int mins[3];
        for (int t = 0; t < 3; ++t) mins[t] = minQtyBoxes[t]->value();
        if (!(mins[0] < mins[1] && mins[1] < mins[2])) {
            QMessageBox::warning(parent, "阶梯设置错误", "三个阶梯的阈值 min_qty 必须严格递增（例如 1 < 5 < 10）。请重新设置。");
            return false;
        }

        // 验证：每档折扣必须 <=1
        for (int t = 0; t < 3; ++t) {
            double dv = discountBoxes[t]->value();
            if (dv > 1.0) {
                QMessageBox::warning(parent, "折扣错误", QString("第%1阶的折扣值不得大于 1。").arg(t+1));
                return false;
            }
        }

        // 构造 outPolicyJson：{ type: "tiered", scope: "global", tiers: [ {min_qty, discount}, ... ] }
        outName = nameEdit->text().toStdString();
        outPolicyJson = nlohmann::json::object();
        outPolicyJson["type"] = "tiered";
        outPolicyJson["scope"] = "global";
        nlohmann::json tiers = nlohmann::json::array();
        for (int t = 0; t < 3; ++t) {
            nlohmann::json tier;
            tier["min_qty"] = mins[t];
            tier["discount"] = discountBoxes[t]->value();
            tiers.push_back(tier);
        }
        outPolicyJson["tiers"] = tiers;
        outPolicyDetail.clear();
        return true;
    }
    else if (kind == PromotionKind::FullReduction) {
        QDoubleSpinBox* threshold = new QDoubleSpinBox;
        threshold->setRange(0.0, 1e9);
        threshold->setDecimals(2);
        QDoubleSpinBox* reduce = new QDoubleSpinBox;
        reduce->setRange(0.0, 1e9);
        reduce->setDecimals(2);
        if (inPolicyJson.contains("threshold")) threshold->setValue(inPolicyJson.value("threshold", 0.0));
        if (inPolicyJson.contains("reduce")) reduce->setValue(inPolicyJson.value("reduce", 0.0));
        form->addRow(new QLabel("满 X（阈值）:"), threshold);
        form->addRow(new QLabel("减 Y（金额）:"), reduce);

        main->addLayout(form);
        QHBoxLayout* hb = new QHBoxLayout;
        QPushButton* ok = new QPushButton("确定");
        QPushButton* cancel = new QPushButton("取消");
        hb->addStretch(); hb->addWidget(ok); hb->addWidget(cancel);
        main->addLayout(hb);
        QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
        QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted) return false;
        outName = nameEdit->text().toStdString();
        outPolicyJson = nlohmann::json::object();
        outPolicyJson["type"] = "full_reduction";
        outPolicyJson["threshold"] = threshold->value();
        outPolicyJson["reduce"] = reduce->value();
        outPolicyJson["scope"] = "global";
        outPolicyDetail.clear();
        return true;
    }

    QTextEdit* policyEdit = new QTextEdit(QString::fromStdString(inPolicyDetail));
    form->addRow(new QLabel("policy 文本:"), policyEdit);
    main->addLayout(form);
    QHBoxLayout* hb = new QHBoxLayout;
    QPushButton* ok = new QPushButton("确定");
    QPushButton* cancel = new QPushButton("取消");
    hb->addStretch(); hb->addWidget(ok); hb->addWidget(cancel);
    main->addLayout(hb);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return false;
    outName = nameEdit->text().toStdString();
    outPolicyDetail = policyEdit->toPlainText().toStdString();
    outPolicyJson = nlohmann::json();
    return true;
}

// 修改 onAddPromotion：先询问类型再编辑
void AdminWindow::onAddPromotion() {
    if (!client_) return;
    PromotionKind kind = PromotionKind::Unknown;
    if (!showPromotionTypeSelector(this, kind)) return;

    std::string outName, outDetail;
    nlohmann::json outPolicyJson;
    // empty inputs for add
    if (!showTypedPromotionEditor(this, kind, "", nlohmann::json(), "", outName, outPolicyJson, outDetail)) return;

    nlohmann::json req;
    req["name"] = outName;
    req["type"] = outPolicyJson.value("type", std::string(""));
    if (!outPolicyJson.is_null() && !outPolicyJson.empty()) req["policy"] = outPolicyJson;
    else if (!outDetail.empty()) req["policy_detail"] = outDetail;

    bool ok = client_->CLTaddPromotion(req);
    QMessageBox::information(this, "新增促销", ok ? "添加成功" : "添加失败（查看日志）");
    refreshPromotions();
}

// 修改 onEditPromotion：读取现有 policy 试图推断类型并预填编辑器
void AdminWindow::onEditPromotion() {
    auto sel = promoTable->selectionModel()->selectedRows();
    if (sel.empty()) { QMessageBox::warning(this, "编辑促销", "请先选择一条促销"); return; }
    int row = sel.first().row();
    QString nameQ = promoTable->item(row, 1)->text();
    QString typeQ = promoTable->item(row, 2)->text();
    QString policyFull = promoTable->item(row, 3)->data(Qt::UserRole).toString();

    // 解析现有 policy：尝试解析 JSON
    nlohmann::json parsed;
    std::string policyDetail;
    PromotionKind kind = PromotionKind::Unknown;
    try {
        parsed = nlohmann::json::parse(policyFull.toStdString());
        if (parsed.is_object()) {
            std::string t = parsed.value("type", std::string(""));
            if (t == "discount") kind = PromotionKind::Discount;
            else if (t == "tiered") kind = PromotionKind::Tiered;
            else if (t == "full_reduction" || t == "fullReduction") kind = PromotionKind::FullReduction;
            else kind = PromotionKind::Unknown;
        } else {
            policyDetail = policyFull.toStdString();
        }
    } catch (...) {
        policyDetail = policyFull.toStdString();
    }

    // 如果无法推断类型，先让管理员选
    if (kind == PromotionKind::Unknown) {
        if (!showPromotionTypeSelector(this, kind)) return;
    }

    std::string newName;
    nlohmann::json newPolicyJson;
    std::string newPolicyDetail;
    if (!showTypedPromotionEditor(this, kind, nameQ.toStdString(), parsed, policyDetail, newName, newPolicyJson, newPolicyDetail)) return;

    nlohmann::json req;
    req["type"] = newPolicyJson.is_null() ? typeQ.toStdString() : newPolicyJson.value("type", typeQ.toStdString());
    if (!newPolicyJson.is_null() && !newPolicyJson.empty()) req["policy"] = newPolicyJson;
    else if (!newPolicyDetail.empty()) req["policy_detail"] = newPolicyDetail;

    bool ok = client_->CLTupdatePromotion(nameQ.toStdString(), req);
    QMessageBox::information(this, "编辑促销", ok ? "更新成功" : "更新失败（查看日志）");
    refreshPromotions();
}

// 确保实现 AdminWindow::refreshPromotions 与 AdminWindow::onDeletePromotion（避免链接缺失）
void AdminWindow::refreshPromotions() {
    if (!client_) return;
    promoTable->setRowCount(0);
    auto rows = client_->CLTgetAllPromotionsRaw();
    promoTable->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < (int)rows.size(); ++i) {
        const auto& r = rows[i];
        QString id = QString::fromStdString(r.value("id", std::string("")));
        QString name = QString::fromStdString(r.value("name", std::string("")));
        QString type;
        try {
            if (r.contains("policy") && r["policy"].is_object()) {
                type = QString::fromStdString(r["policy"].value("type", std::string("")));
            } else {
                type = QString::fromStdString(r.value("type", std::string("")));
            }
        } catch (...) {
            type = QString::fromStdString(r.value("type", std::string("")));
        }

        QString policyStr;
        try {
            if (r.contains("policy")) policyStr = QString::fromStdString(r["policy"].dump());
            else policyStr = QString::fromStdString(r.value("policy_detail", std::string("")));
        } catch (...) {
            policyStr = QString::fromStdString(r.value("policy_detail", std::string("")));
        }

        QTableWidgetItem* idItem = new QTableWidgetItem(id);
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        QTableWidgetItem* typeItem = new QTableWidgetItem(type);
        QTableWidgetItem* policyItem = new QTableWidgetItem(policyStr.left(200)); // 表格仅显示摘要
        policyItem->setData(Qt::UserRole, policyStr); // 将完整 policy 存到 UserRole 备用

        promoTable->setItem(i, 0, idItem);
        promoTable->setItem(i, 1, nameItem);
        promoTable->setItem(i, 2, typeItem);
        promoTable->setItem(i, 3, policyItem);
    }
    Logger::instance().info(std::string("AdminWindow::refreshPromotions loaded ") + std::to_string(rows.size()) + " promotions");
}

void AdminWindow::onDeletePromotion() {
    if (!client_) return;
    auto sel = promoTable->selectionModel()->selectedRows();
    if (sel.empty()) {
        QMessageBox::warning(this, "删除促销", "请先选择一条促销");
        return;
    }
    int row = sel.first().row();
    if (!promoTable->item(row, 1)) {
        QMessageBox::warning(this, "删除促销", "所选行无有效名称列");
        return;
    }
    QString nameQ = promoTable->item(row, 1)->text();
    if (QMessageBox::question(this, "删除促销", QString("确定要删除促销 “%1” 吗？").arg(nameQ)) != QMessageBox::Yes) return;

    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("AdminWindow::onDeletePromotion: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    bool ok = client_->CLTdeletePromotion(nameQ.toStdString());
    QMessageBox::information(this, "删除促销", ok ? "删除成功" : "删除失败（查看日志）");
    refreshPromotions();
}