#include "AdminWindow.h"
#include "logger.h"
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>

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
    ordersTable->setColumnCount(4);
    ordersTable->setHorizontalHeaderLabels(QStringList() << "订单ID" << "用户手机号" << "状态" << "摘要");
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

    // 首次加载
    refreshUsers();
    refreshGoods();
    refreshOrders();
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

void AdminWindow::refreshOrders() {
    ordersTable->setRowCount(0);
    if (!client_) return;
    auto orders = client_->CLTgetAllOrders(); // 获取所有订单
    ordersTable->setRowCount((int)orders.size());
    for (int i = 0; i < (int)orders.size(); ++i) {
        const Order& o = orders[i];
        ordersTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(o.getOrderId())));
        ordersTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(o.getUserPhone())));
        ordersTable->setItem(i, 2, new QTableWidgetItem(orderStatusToText(o.getStatus())));
        QString summary = QString("总计 %1").arg(o.getTotalAmount());
        ordersTable->setItem(i, 3, new QTableWidgetItem(summary));
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

    // 构造显示文本
    QString txt;
    txt += "订单ID: " + QString::fromStdString(detail.getOrderId()) + "\n";
    txt += "手机号: " + QString::fromStdString(detail.getUserPhone()) + "\n";
    txt += "地址: " + QString::fromStdString(detail.getShippingAddress()) + "\n";
    txt += "状态: " + orderStatusToText(detail.getStatus()) + "\n";
    txt += "总额: " + QString::number(detail.getTotalAmount()) + "  实付: " + QString::number(detail.getFinalAmount()) + "\n\n";
    txt += "订单项:\n";
    const auto& itemsVec = detail.getItems();
    if (itemsVec.empty()) {
        txt += "(无订单项)\n";
    } else {
        for (const auto& it : itemsVec) {
            txt += QString::fromStdString(it.getGoodName()) + " (id:" + QString::number(it.getGoodId()) + ") x" + QString::number(it.getQuantity())
                   + " 单价:" + QString::number(it.getPrice()) + " 小计:" + QString::number(it.getSubtotal()) + "\n";
        }
    }
    QMessageBox::information(this, "订单详情", txt);
}