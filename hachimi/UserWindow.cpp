#include "UserWindow.h"
#include "logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QDateTime>
#include <QFormLayout>
#include <random>
// 在文件顶部 includes 之后加入与 AdminWindow 相同的状态映射辅助函数（UI 文件内局部）
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

UserWindow::UserWindow(const std::string& phone, Client* client, QWidget* parent)
    : QWidget(parent), phone_(phone), client_(client)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    tabWidget = new QTabWidget(this);

    // 1. 用户信息管理
    userTab = new QWidget;
    QVBoxLayout* userLayout = new QVBoxLayout(userTab);

    QFormLayout* form = new QFormLayout;
    phoneEdit = new QLineEdit;
    phoneEdit->setReadOnly(true);
    phoneEdit->setText(QString::fromStdString(phone_));
    passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    addressEdit = new QLineEdit;

    form->addRow(new QLabel("手机号:"), phoneEdit);
    form->addRow(new QLabel("密码:"), passwordEdit);
    form->addRow(new QLabel("地址:"), addressEdit);

    userLayout->addLayout(form);

    QHBoxLayout* userBtnLayout = new QHBoxLayout;
    saveInfoBtn = new QPushButton("保存修改");
    deleteAccountBtn = new QPushButton("注销账号（删除）");
    userBtnLayout->addWidget(saveInfoBtn);
    userBtnLayout->addWidget(deleteAccountBtn);
    userLayout->addLayout(userBtnLayout);

    tabWidget->addTab(userTab, "个人信息");

    // 2. 商品浏览与购物车
    goodsTab = new QWidget;
    QVBoxLayout* goodsLayout = new QVBoxLayout(goodsTab);

    goodsTable = new QTableWidget;
    goodsTable->setColumnCount(5);
    goodsTable->setHorizontalHeaderLabels(QStringList() << "ID" << "名称" << "价格" << "库存" << "分类");
    goodsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    goodsLayout->addWidget(goodsTable);

    QHBoxLayout* goodsBtnLayout = new QHBoxLayout;
    refreshGoodsBtn = new QPushButton("刷新商品");
    addToCartBtn = new QPushButton("加入购物车");
    goodsBtnLayout->addWidget(refreshGoodsBtn);
    goodsBtnLayout->addWidget(addToCartBtn);
    goodsLayout->addLayout(goodsBtnLayout);

    // 购物车视图
    cartTable = new QTableWidget;
    cartTable->setColumnCount(5);
    cartTable->setHorizontalHeaderLabels(QStringList() << "商品ID" << "名称" << "单价" << "数量" << "小计");
    cartTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    goodsLayout->addWidget(new QLabel("购物车："));
    goodsLayout->addWidget(cartTable);

    QHBoxLayout* cartBtnLayout = new QHBoxLayout;
    refreshCartBtn = new QPushButton("刷新购物车");
    modifyCartBtn = new QPushButton("修改数量");
    removeCartBtn = new QPushButton("删除商品");
    checkoutBtn = new QPushButton("结算（生成订单）");
    cartBtnLayout->addWidget(refreshCartBtn);
    cartBtnLayout->addWidget(modifyCartBtn);
    cartBtnLayout->addWidget(removeCartBtn);
    cartBtnLayout->addWidget(checkoutBtn);
    goodsLayout->addLayout(cartBtnLayout);

    tabWidget->addTab(goodsTab, "商品与购物车");

    // 3. 订单查看
    orderTab = new QWidget;
    QVBoxLayout* orderLayout = new QVBoxLayout(orderTab);
    orderTable = new QTableWidget;
    // 删除显示手机号列：只保留 订单ID, 状态, 实付金额 三列
    orderTable->setColumnCount(3);
    orderTable->setHorizontalHeaderLabels(QStringList() << "订单ID" << "状态" << "实付金额");
    orderTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    orderLayout->addWidget(orderTable);

    QHBoxLayout* orderBtnLayout = new QHBoxLayout;
    refreshOrdersBtn = new QPushButton("刷新订单");
    viewOrderBtn = new QPushButton("查看详情");
    returnOrderBtn = new QPushButton("退货");
    repairOrderBtn = new QPushButton("维修");
    deleteOrderBtn = new QPushButton("删除订单");
    orderBtnLayout->addWidget(refreshOrdersBtn);
    orderBtnLayout->addWidget(viewOrderBtn);
    orderBtnLayout->addWidget(returnOrderBtn);
    orderBtnLayout->addWidget(repairOrderBtn);
    orderBtnLayout->addWidget(deleteOrderBtn);
    orderLayout->addLayout(orderBtnLayout);

    tabWidget->addTab(orderTab, "我的订单");

    mainLayout->addWidget(tabWidget);

    // 返回按钮及“返回身份选择界面”按钮，横向排列
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout;
    backBtn = new QPushButton("返回上一级", this);
    returnIdentityBtn = new QPushButton("返回身份选择界面", this);
    bottomBtnLayout->addStretch();
    bottomBtnLayout->addWidget(backBtn);
    bottomBtnLayout->addWidget(returnIdentityBtn);
    mainLayout->addLayout(bottomBtnLayout);

    // 连接槽
    connect(backBtn, &QPushButton::clicked, this, &UserWindow::close);
    connect(returnIdentityBtn, &QPushButton::clicked, this, &UserWindow::onReturnToIdentitySelection);

    connect(refreshGoodsBtn, &QPushButton::clicked, this, &UserWindow::refreshGoods);
    connect(addToCartBtn, &QPushButton::clicked, this, &UserWindow::onAddToCart);
    connect(refreshCartBtn, &QPushButton::clicked, this, &UserWindow::refreshCart);
    connect(modifyCartBtn, &QPushButton::clicked, this, &UserWindow::onModifyCartItem);
    connect(removeCartBtn, &QPushButton::clicked, this, &UserWindow::onRemoveCartItem);
    connect(checkoutBtn, &QPushButton::clicked, this, &UserWindow::onCheckout);

    // 新增连接：保存与注销
    connect(saveInfoBtn, &QPushButton::clicked, this, &UserWindow::onSaveUserInfo);
    connect(deleteAccountBtn, &QPushButton::clicked, this, &UserWindow::onDeleteAccount);

    connect(refreshOrdersBtn, &QPushButton::clicked, this, &UserWindow::refreshOrders);
    connect(viewOrderBtn, &QPushButton::clicked, this, &UserWindow::onViewOrderDetail);
    connect(returnOrderBtn, &QPushButton::clicked, this, &UserWindow::onReturnOrder);
    connect(repairOrderBtn, &QPushButton::clicked, this, &UserWindow::onRepairOrder);
    connect(deleteOrderBtn, &QPushButton::clicked, this, &UserWindow::onDeleteOrder);

    // 初始加载
    refreshGoods();
    refreshCart();
    refreshOrders();

    // 尝试从客户端加载当前用户信息（若可用）
    if (client_) {
        auto users = client_->CLTgetAllAccounts();
        for (const auto& u : users) {
            if (u.getPhone() == phone_) {
                currentPassword_ = u.getPassword();
                // 将密码字段留空（安全），但保留当前密码以便“未修改密码时保持不变”的行为
                addressEdit->setText(QString::fromStdString(u.getAddress()));
                break;
            }
        }
        // 额外保证：即便上面没找到用户，也把构造时传入的 phone_ 显示到界面
        if (!phone_.empty()) {
            phoneEdit->setText(QString::fromStdString(phone_));
        }
    }
    else {
        // 无 client 时禁用编辑与注销
        saveInfoBtn->setEnabled(false);
        deleteAccountBtn->setEnabled(false);
        returnIdentityBtn->setEnabled(false);
    }
}

void UserWindow::refreshGoods() {
    goodsTable->setRowCount(0);
    if (!client_) return;
    auto goods = client_->CLTgetAllGoods();
    goodsTable->setRowCount((int)goods.size());
    for (int i = 0; i < (int)goods.size(); ++i) {
        const Good& g = goods[i];
        // 使用访问器而非直接访问字段
        goodsTable->setItem(i, 0, new QTableWidgetItem(QString::number(g.getId())));
        goodsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(g.getName())));
        goodsTable->setItem(i, 2, new QTableWidgetItem(QString::number(g.getPrice())));
        goodsTable->setItem(i, 3, new QTableWidgetItem(QString::number(g.getStock())));
        goodsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(g.getCategory())));
    }
}

void UserWindow::onAddToCart() {
    if (!client_) return;
    auto items = goodsTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "加入购物车", "请先选择商品");
        return;
    }
    int row = goodsTable->row(items.first());
    int id = goodsTable->item(row, 0)->text().toInt();
    QString name = goodsTable->item(row, 1)->text();
    double price = goodsTable->item(row, 2)->text().toDouble();

    bool ok;
    int qty = QInputDialog::getInt(this, "加入购物车", "数量:", 1, 1, 1000000, 1, &ok);
    if (!ok) return;

    // 详细日志：记录准备发送的参数
    Logger::instance().info(std::string("UserWindow::onAddToCart params: phone=") + phone_ +
                            ", goodId=" + std::to_string(id) +
                            ", name=" + name.toStdString() +
                            ", price=" + std::to_string(price) +
                            ", qty=" + std::to_string(qty));

    // 若连接断开，尝试重连一次并记录结果
    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("UserWindow::onAddToCart: client not connected, attempting reconnect");
        bool rc = client_->CLTreconnect();
        Logger::instance().info(std::string("UserWindow::onAddToCart: CLTreconnect returned ") + (rc ? "true" : "false"));
        if (!rc) {
            QMessageBox::warning(this, "加入购物车", "与服务器连接失败，无法加入购物车（已尝试重连）");
            return;
        }
    }

    // 发请求并记录返回值
    bool res = client_->CLTaddToCart(phone_, id, name.toStdString(), price, qty);
    Logger::instance().info(std::string("UserWindow::onAddToCart CLTaddToCart returned ") + (res ? "true" : "false"));

    if (res) {
        QMessageBox::information(this, "加入购物车", "已加入购物车");
    } else {
        // 更具体的提示并引导查看日志
        QMessageBox::warning(this, "加入购物车", "加入失败（查看 log.txt 获取详细信息）");
    }

    // 刷新显示（即便失败也尝试刷新）
    refreshCart();
}

void UserWindow::refreshCart() {
    cartTable->setRowCount(0);
    if (!client_) {
        Logger::instance().warn("UserWindow::refreshCart: client_ is null");
        return;
    }

    Logger::instance().info(std::string("UserWindow::refreshCart: client connection active? ") + (client_->CLTisConnectionActive() ? "true" : "false"));
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    Logger::instance().info(std::string("UserWindow::refreshCart: got cart_id=") + cart.cart_id + ", items=" + std::to_string(cart.items.size()));

    if (cart.cart_id.empty() && cart.items.empty()) {
        Logger::instance().info("UserWindow::refreshCart: cart is empty or not found for user " + phone_);
    }

    cartTable->setRowCount((int)cart.items.size());
    for (int i = 0; i < (int)cart.items.size(); ++i) {
        const CartItem& it = cart.items[i];
        Logger::instance().info(std::string("UserWindow::refreshCart: item ") + std::to_string(i) + " good_id=" + std::to_string(it.good_id) + ", name=" + it.good_name + ", qty=" + std::to_string(it.quantity) + ", subtotal=" + std::to_string(it.subtotal));
        cartTable->setItem(i, 0, new QTableWidgetItem(QString::number(it.good_id)));
        cartTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(it.good_name)));
        cartTable->setItem(i, 2, new QTableWidgetItem(QString::number(it.price)));
        cartTable->setItem(i, 3, new QTableWidgetItem(QString::number(it.quantity)));
        cartTable->setItem(i, 4, new QTableWidgetItem(QString::number(it.subtotal)));
    }
}

void UserWindow::onCheckout() {
    if (!client_) return;

    // 后备：若 phone_ 为空则从界面读取（防止构造时未传入）
    if (phone_.empty()) {
        phone_ = phoneEdit->text().toStdString();
        Logger::instance().info(std::string("UserWindow::onCheckout: phone_ was empty, fallback to phoneEdit: ") + phone_);
    }

    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    if (cart.items.empty()) {
        QMessageBox::information(this, "结算", "购物车为空");
        return;
    }

    // 不再直接复用 cart.cart_id（可能已使用），改为客户端生成唯一订单 id
    // 格式：o<timestamp_ms>_<rand>
    qint64 ms = QDateTime::currentMSecsSinceEpoch();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;
    uint32_t r = dist(gen);
    QString orderId = QString("o%1_%2").arg(ms).arg(r, 8, 16, QChar('0'));

    Order order(cart, orderId.toStdString(), 1);
    // 确保 order 带上手机号
    if (order.getUserPhone().empty()) order.setUserPhone(phone_);

    // 记录发送内容（便于排查）
    Logger::instance().info(std::string("UserWindow::onCheckout: sending ADD_SETTLED_ORDER for orderId=") + orderId.toStdString() + " userPhone=" + phone_);

    bool ok = client_->CLTaddSettledOrder(order);
    Logger::instance().info(std::string("UserWindow::onCheckout: CLTaddSettledOrder returned ") + (ok ? "true" : "false"));

    if (ok) {
        QMessageBox::information(this, "结算", "结算成功（已提交订单）");
        // 清空购物车：用后台接口删除购物车内商品
        for (const auto& it : cart.items) {
            client_->CLTremoveFromCart(phone_, it.good_id);
        }
        refreshCart();
    } else {
        QMessageBox::warning(this, "结算", "结算失败，请查看 log.txt 获取详细信息");
    }
}

// 保存用户信息（密码/地址），手机号不可修改
void UserWindow::onSaveUserInfo() {
    Logger::instance().info("UserWindow::onSaveUserInfo called");
    if (!client_) return;

    QString uiPwd = passwordEdit->text().trimmed();   // new password entered by user (may be empty)
    QString uiAddr = addressEdit->text().trimmed();   // address field (may be empty meaning "no change")

    // 若连接断开，尝试重连一次
    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("UserWindow::onSaveUserInfo: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    // 获取服务器上当前用户信息（用于判断地址是否被修改 / 填充空地址）
    QString serverAddr;
    {
        auto users = client_->CLTgetAllAccounts();
        for (const auto& u : users) {
            if (u.getPhone() == phone_) {
                serverAddr = QString::fromStdString(u.getAddress());
                break;
            }
        }
    }

    // 判断用户是否真正修改了地址：
    bool addressModified = false;
    QString newAddr = uiAddr;
    if (uiAddr.isEmpty()) {
        // 空表示不修改地址，使用服务器当前地址作为占位（不视为修改）
        newAddr = serverAddr;
        addressModified = false;
    } else {
        // 非空且不同于服务器上的地址视为修改
        addressModified = (uiAddr != serverAddr);
        newAddr = uiAddr;
    }

    // 情况 A: 未填写新密码 —— 仅更新地址（可能没改则也会调用，但服务器会更新或保持不变）
    if (uiPwd.isEmpty()) {
        // 地址最终不能为空（服务器上也可能为空），做个基本检查
        if (newAddr.isEmpty()) {
            QMessageBox::warning(this, "保存修改", "地址不能为空（请在地址栏输入或先在服务器端设置地址）");
            return;
        }
        // 使用当前已知密码（currentPassword_）进行认证并更新
        bool res = client_->CLTupdateUser(phone_, currentPassword_, newAddr.toStdString());
        Logger::instance().info(std::string("UserWindow::onSaveUserInfo CLTupdateUser(return) ") + (res ? "true" : "false"));
        if (res) {
            QMessageBox::information(this, "保存修改", "保存成功");
            // 不改 currentPassword_
            passwordEdit->clear();
        } else {
            QMessageBox::warning(this, "保存修改", "保存失败，请检查网络或稍后重试");
        }
        return;
    }

    // 情况 B: 填写了新密码 —— 需要输入旧密码进行验证
    bool ok;
    QString oldPwd = QInputDialog::getText(this, "验证原密码", "请输入原密码以确认修改密码:", QLineEdit::Password, "", &ok);
    if (!ok) return;
    if (oldPwd.isEmpty()) {
        QMessageBox::warning(this, "保存修改", "请输入原密码以确认修改");
        return;
    }

    // 分两条路径：
    // B1: 仅改密码（地址未被修改） => 直接调用修改密码接口
    if (!addressModified) {
        bool passChanged = client_->CLTupdateAccountPassword(phone_, oldPwd.toStdString(), uiPwd.toStdString());
        Logger::instance().info(std::string("UserWindow::onSaveUserInfo CLTupdateAccountPassword returned ") + (passChanged ? "true" : "false"));
        if (!passChanged) {
            QMessageBox::warning(this, "修改密码", "修改密码失败（原密码错误或服务器错误）");
            return;
        }
        // 密码改成功
        currentPassword_ = uiPwd.toStdString();
        passwordEdit->clear();
        QMessageBox::information(this, "修改密码", "密码已更新");
        return;
    }

    // B2: 同时改地址和密码（或仅认为地址被修改）：
    // 先使用旧密码更新地址（认证并修改地址），再修改密码
    // 先保存当前服务器地址以便回滚（可选）
    QString prevAddr = serverAddr;

    // 1) 用旧密码更新地址
    bool addrUpdated = client_->CLTupdateUser(phone_, oldPwd.toStdString(), newAddr.toStdString());
    Logger::instance().info(std::string("UserWindow::onSaveUserInfo CLTupdateUser(before pwd change) returned ") + (addrUpdated ? "true" : "false"));
    if (!addrUpdated) {
        QMessageBox::warning(this, "保存修改", "地址更新失败（请检查原密码或网络），已取消密码修改流程。");
        return;
    }

    // 2) 修改密码
    bool passChanged = client_->CLTupdateAccountPassword(phone_, oldPwd.toStdString(), uiPwd.toStdString());
    Logger::instance().info(std::string("UserWindow::onSaveUserInfo CLTupdateAccountPassword returned ") + (passChanged ? "true" : "false"));
    if (!passChanged) {
        // 尝试回滚地址到 prevAddr（若可用）
        bool rolledBack = false;
        if (!prevAddr.isEmpty()) {
            rolledBack = client_->CLTupdateUser(phone_, oldPwd.toStdString(), prevAddr.toStdString());
            Logger::instance().info(std::string("UserWindow::onSaveUserInfo rollback CLTupdateUser returned ") + (rolledBack ? "true" : "false"));
        }
        QString msg = "修改密码失败。";
        if (!rolledBack && !prevAddr.isEmpty()) msg += " 注意：地址可能已被修改且回滚失败，请联系管理员处理。";
        QMessageBox::warning(this, "修改密码", msg);
        return;
    }

    // 如果两步都成功
    currentPassword_ = uiPwd.toStdString();
    passwordEdit->clear();
    QMessageBox::information(this, "保存修改", "密码与地址均已更新");
}

// 注销（删除当前手机号的用户）
void UserWindow::onDeleteAccount() {
    Logger::instance().info("UserWindow::onDeleteAccount called");
    if (!client_) return;

    if (QMessageBox::question(this, "注销账号", "确认要注销并删除此账号？此操作不可恢复。") != QMessageBox::Yes) {
        return;
    }

    // 要求用户输入密码以确认删除
    bool ok;
    QString pwd = QInputDialog::getText(this, "验证密码", "请输入账号密码以确认删除:", QLineEdit::Password, "", &ok);
    if (!ok) return;
    if (pwd.isEmpty()) {
        QMessageBox::warning(this, "注销账号", "请输入密码以确认");
        return;
    }

    // 若连接断开，尝试重连一次
    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("UserWindow::onDeleteAccount: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    bool success = client_->CLTdeleteAccount(phone_, pwd.toStdString());
    Logger::instance().info(std::string("UserWindow::onDeleteAccount CLTdeleteAccount returned ") + (success ? "true" : "false"));
    if (success) {
        QMessageBox::information(this, "注销账号", "账号已成功删除，窗口将关闭并返回登录界面。");
        // 发出信号，通知上层（例如 main.cpp）返回登录界面
        emit accountDeleted();
        this->close();
    }
    else {
        QMessageBox::warning(this, "注销账号", "删除账号失败（密码错误或服务器错误）");
    }
}

// 新增：返回身份选择界面槽实现
void UserWindow::onReturnToIdentitySelection() {
    Logger::instance().info("UserWindow::onReturnToIdentitySelection called");
    emit backRequested();
    this->close();
}

// 修改购物车商品数量（已增强日志、重连与重试逻辑）
void UserWindow::onModifyCartItem() {
    Logger::instance().info("UserWindow::onModifyCartItem called");
    if (!client_) {
        Logger::instance().warn("UserWindow::onModifyCartItem: client_ is null");
        return;
    }

    auto items = cartTable->selectedItems();
    if (items.empty()) {
        Logger::instance().warn("UserWindow::onModifyCartItem: no selection");
        QMessageBox::warning(this, "修改数量", "请先选择购物车中的商品行");
        return;
    }

    int row = cartTable->row(items.first());
    if (row < 0) {
        Logger::instance().warn("UserWindow::onModifyCartItem: invalid selected row");
        QMessageBox::warning(this, "修改数量", "无法确定选中行");
        return;
    }

    bool ok;
    int productId = 0;
    int currentQty = 0;
    try {
        productId = cartTable->item(row, 0)->text().toInt();
        currentQty = cartTable->item(row, 3)->text().toInt();
    } catch (...) {
        Logger::instance().warn("UserWindow::onModifyCartItem: failed to read productId/currentQty from table");
        QMessageBox::warning(this, "修改数量", "读取选中行数据失败");
        return;
    }

    Logger::instance().info(std::string("UserWindow::onModifyCartItem: selected productId=") + std::to_string(productId) + ", currentQty=" + std::to_string(currentQty));

    int newQty = QInputDialog::getInt(this, "修改数量", "请输入新数量:", currentQty, 0, 1000000, 1, &ok);
    if (!ok) {
        Logger::instance().info("UserWindow::onModifyCartItem: user cancelled input dialog");
        return;
    }

    Logger::instance().info(std::string("UserWindow::onModifyCartItem: requested newQty=") + std::to_string(newQty));

    // 若 newQty == 0 当作删除
    if (newQty == 0) {
        if (QMessageBox::question(this, "删除商品", "数量设为 0，是否从购物车删除该商品？") != QMessageBox::Yes) {
            Logger::instance().info("UserWindow::onModifyCartItem: user declined delete-on-zero");
            return;
        }
        // 调用删除逻辑（含重试）
        int attempts = 0;
        bool removed = false;
        for (; attempts < 2; ++attempts) {
            if (!client_->CLTisConnectionActive()) {
                Logger::instance().warn("UserWindow::onModifyCartItem: client not connected, attempting reconnect");
                bool rc = client_->CLTreconnect();
                Logger::instance().info(std::string("UserWindow::onModifyCartItem: CLTreconnect returned ") + (rc ? "true" : "false"));
                if (!rc) { Logger::instance().warn("UserWindow::onModifyCartItem: reconnect failed"); }
            }
            Logger::instance().info(std::string("UserWindow::onModifyCartItem: attempt remove productId=") + std::to_string(productId) + ", attempt=" + std::to_string(attempts+1));
            removed = client_->CLTremoveFromCart(phone_, productId);
            Logger::instance().info(std::string("UserWindow::onModifyCartItem CLTremoveFromCart returned ") + (removed ? "true" : "false"));
            if (removed) break;
        }
        if (removed) {
            QMessageBox::information(this, "删除商品", "已从购物车删除");
        } else {
            QMessageBox::warning(this, "删除商品", "删除失败（查看日志）");
        }
        refreshCart();
        return;
    }

    // 否则更新数量（重试一次）
    int attempts = 0;
    bool updated = false;
    for (; attempts < 2; ++attempts) {
        if (!client_->CLTisConnectionActive()) {
            Logger::instance().warn("UserWindow::onModifyCartItem: client not connected, attempting reconnect");
            bool rc = client_->CLTreconnect();
            Logger::instance().info(std::string("UserWindow::onModifyCartItem: CLTreconnect returned ") + (rc ? "true" : "false"));
            if (!rc) { Logger::instance().warn("UserWindow::onModifyCartItem: reconnect failed"); }
        }
        Logger::instance().info(std::string("UserWindow::onModifyCartItem: attempt update productId=") + std::to_string(productId) + ", newQty=" + std::to_string(newQty) + ", attempt=" + std::to_string(attempts+1));
        updated = client_->CLTupdateCartItem(phone_, productId, newQty);
        Logger::instance().info(std::string("UserWindow::onModifyCartItem CLTupdateCartItem returned ") + (updated ? "true" : "false"));
        if (updated) break;
    }

    if (updated) {
        QMessageBox::information(this, "修改数量", "已更新数量");
    } else {
        QMessageBox::warning(this, "修改数量", "更新失败（查看日志）");
    }
    refreshCart();
}

// 删除购物车商品（已增强日志、重连与重试逻辑）
void UserWindow::onRemoveCartItem() {
    Logger::instance().info("UserWindow::onRemoveCartItem called");
    if (!client_) {
        Logger::instance().warn("UserWindow::onRemoveCartItem: client_ is null");
        return;
    }

    auto items = cartTable->selectedItems();
    if (items.empty()) {
        Logger::instance().warn("UserWindow::onRemoveCartItem: no selection");
        QMessageBox::warning(this, "删除商品", "请先选择购物车中的商品行");
        return;
    }
    int row = cartTable->row(items.first());
    if (row < 0) {
        Logger::instance().warn("UserWindow::onRemoveCartItem: invalid selected row");
        QMessageBox::warning(this, "删除商品", "无法确定选中行");
        return;
    }

    int productId = 0;
    try {
        productId = cartTable->item(row, 0)->text().toInt();
    } catch (...) {
        Logger::instance().warn("UserWindow::onRemoveCartItem: failed to parse productId");
        QMessageBox::warning(this, "删除商品", "读取选中行失败");
        return;
    }

    Logger::instance().info(std::string("UserWindow::onRemoveCartItem: selected productId=") + std::to_string(productId));

    if (QMessageBox::question(this, "删除商品", "确认从购物车删除该商品？") != QMessageBox::Yes) {
        Logger::instance().info("UserWindow::onRemoveCartItem: user cancelled deletion");
        return;
    }

    int attempts = 0;
    bool removed = false;
    for (; attempts < 2; ++attempts) {
        if (!client_->CLTisConnectionActive()) {
            Logger::instance().warn("UserWindow::onRemoveCartItem: client not connected, attempting reconnect");
            bool rc = client_->CLTreconnect();
            Logger::instance().info(std::string("UserWindow::onRemoveCartItem: CLTreconnect returned ") + (rc ? "true" : "false"));
            if (!rc) { Logger::instance().warn("UserWindow::onRemoveCartItem: reconnect failed"); }
        }
        Logger::instance().info(std::string("UserWindow::onRemoveCartItem: attempt remove productId=") + std::to_string(productId) + ", attempt=" + std::to_string(attempts+1));
        removed = client_->CLTremoveFromCart(phone_, productId);
        Logger::instance().info(std::string("UserWindow::onRemoveCartItem CLTremoveFromCart returned ") + (removed ? "true" : "false"));
        if (removed) break;
    }

    if (removed) {
        QMessageBox::information(this, "删除商品", "已从购物车删除");
    } else {
        QMessageBox::warning(this, "删除商品", "删除失败（查看日志）");
    }
    refreshCart();
}

// ----------------- 新增槽实现：订单相关 -----------------

void UserWindow::refreshOrders() {
    if (!client_) return;
    orderTable->setRowCount(0);
    auto orders = client_->CLTgetAllOrders(phone_);
    orderTable->setRowCount((int)orders.size());
    for (int i = 0; i < (int)orders.size(); ++i) {
        const Order& o = orders[i];
        // 订单ID 列 —— 将 userPhone 隐藏存为 UserRole 以备需要，但不显示
        QTableWidgetItem* idItem = new QTableWidgetItem(QString::fromStdString(o.getOrderId()));
        idItem->setData(Qt::UserRole, QString::fromStdString(o.getUserPhone()));
        orderTable->setItem(i, 0, idItem);

        // 状态列
        orderTable->setItem(i, 1, new QTableWidgetItem(orderStatusToText(o.getStatus())));

        // 实付金额列（使用 final amount）
        orderTable->setItem(i, 2, new QTableWidgetItem(QString::number(o.getFinalAmount())));
    }
}

void UserWindow::onViewOrderDetail() {
    if (!client_) return;
    auto items = orderTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "查看订单", "请先选择订单行"); return; }
    int row = orderTable->row(items.first());
    QString oid = orderTable->item(row, 0)->text();
    Order detail;
    bool ok = client_->CLTgetOrderDetail(oid.toStdString(), phone_, detail);
    if (!ok) { QMessageBox::warning(this, "查看订单", "获取订单详情失败"); return; }
    // 构造显示文本
    QString txt;
    txt += "订单ID: " + QString::fromStdString(detail.getOrderId()) + "\n";
    txt += "手机号: " + QString::fromStdString(detail.getUserPhone()) + "\n";
    txt += "地址: " + QString::fromStdString(detail.getShippingAddress()) + "\n";
    txt += "状态: " + QString::number(detail.getStatus()) + "\n";
    txt += "总额: " + QString::number(detail.getTotalAmount()) + "  实付: " + QString::number(detail.getFinalAmount()) + "\n\n";
    txt += "订单项:\n";
    for (const auto& it : detail.getItems()) {
        txt += QString::fromStdString(it.getGoodName()) + " (id:" + QString::number(it.getGoodId()) + ") x" + QString::number(it.getQuantity())
               + " 单价:" + QString::number(it.getPrice()) + " 小计:" + QString::number(it.getSubtotal()) + "\n";
    }
    QMessageBox::information(this, "订单详情", txt);
}

void UserWindow::onReturnOrder() {
    if (!client_) return;
    auto items = orderTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "退货", "请先选择订单行"); return; }
    int row = orderTable->row(items.first());
    QString oid = orderTable->item(row, 0)->text();
    if (QMessageBox::question(this, "退货", "确认对订单 " + oid + " 发起退货请求？") != QMessageBox::Yes) return;
    bool ok = client_->CLTreturnSettledOrder(oid.toStdString(), phone_);
    QMessageBox::information(this, "退货", ok ? "退货请求已发送" : "退货失败（查看日志）");
    refreshOrders();
}

void UserWindow::onRepairOrder() {
    if (!client_) return;
    auto items = orderTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "维修", "请先选择订单行"); return; }
    int row = orderTable->row(items.first());
    QString oid = orderTable->item(row, 0)->text();
    if (QMessageBox::question(this, "维修", "确认对订单 " + oid + " 发起维修请求？") != QMessageBox::Yes) return;
    bool ok = client_->CLTrepairSettledOrder(oid.toStdString(), phone_);
    QMessageBox::information(this, "维修", ok ? "维修请求已发送" : "维修失败（查看日志）");
    refreshOrders();
}

void UserWindow::onDeleteOrder() {
    if (!client_) return;
    auto items = orderTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "删除订单", "请先选择订单行"); return; }
    int row = orderTable->row(items.first());
    QString oid = orderTable->item(row, 0)->text();
    if (QMessageBox::question(this, "删除订单", "确认删除订单 " + oid + " ? 该操作可能不可恢复") != QMessageBox::Yes) return;
    bool ok = client_->CLTdeleteSettledOrder(oid.toStdString(), phone_);
    QMessageBox::information(this, "删除订单", ok ? "删除成功" : "删除失败（查看日志）");
    refreshOrders();
}