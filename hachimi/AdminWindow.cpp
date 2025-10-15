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
#include <QApplication> // 用于切换主题
#include <QDateTimeEdit> // 新增：用于订单筛选的日期时间选择
#include "Theme.h"

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

// 判断系统当前是否为深色主题（基于 QPalette::Window 的 lightness）
static bool isSystemDarkTheme() {
    QColor bg = qApp->palette().color(QPalette::Window);
    // lightness 范围 0..255，128 为中间阈值
    return bg.lightness() < 128;
}

// 主题切换支持：应用简单的暗黑样式或恢复默认（浅色）
// 注意：s_darkMode_AdminWindow 在构造函数中初始化为系统当前主题，
// 但用户可以通过“切换主题”按钮在浅/深之间切换（不再受系统强制）。
static bool s_darkMode_AdminWindow = false;
static void applyTheme_AdminWindow(bool dark) {
    if (dark) {
        QString ss = R"(
            /* 深色主题：深海背景 + 柔和青绿色强调 */
            QWidget { background-color: #0B1A1E; color: #E6F1F2; }

            /* 标签页 */
            QTabWidget::pane { background: transparent; }
            QTabBar::tab { background: transparent; padding: 6px 12px; color: #CFEFEA; }
            QTabBar::tab:selected { background: rgba(45,212,191,0.10); border-radius:4px; }

            /* 输入控件 */
            QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
                background-color: #0F2A2E;
                color: #E6F1F2;
                border: 1px solid #1E3A3E;
                border-radius: 4px;
                padding: 4px;
            }
            QLineEdit:focus, QTextEdit:focus, QComboBox:focus {
                border: 1px solid #2DD4BF;
            }

            /* 表格 */
            QTableWidget {
                background-color: #071014;
                color: #E6F1F2;
                gridline-color: #122B2E;
                selection-background-color: rgba(45,212,191,0.14);
                selection-color: #E6F1F2;
            }
            QHeaderView::section {
                background-color: #0F2A2E;
                color: #CFEFEA;
                padding: 4px;
                border: 1px solid #122B2E;
            }

            /* 按钮 */
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #175656, stop:1 #0F3B3E);
                color: #F8FFFE;
                border: 1px solid #1E6E69;
                border-radius: 4px;
                padding: 6px 10px;
            }
            QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1f7f78, stop:1 #126c68); }
            QPushButton:pressed { background-color: #0b5b55; }

            /* 提示与对话框 */
            QMessageBox { background-color: #071014; color: #E6F1F2; }
            QMenu { background-color: #071014; color: #E6F1F2; }
            QToolTip { background-color: #0f2a2e; color: #E6F1F2; border: 1px solid #1E3A3E; }
        )";
        qApp->setStyleSheet(ss);
    }
    else {
        qApp->setStyleSheet(QString());
    }
    s_darkMode_AdminWindow = dark;
    Logger::instance().info(std::string("AdminWindow: theme switched to ") + (dark ? "dark" : "light"));
}

// helper: case-insensitive substring
static bool containsIC_admin(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = haystack;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// 节流实现：每秒最多允许一次（首次允许）
bool AdminWindow::tryThrottle(QWidget* parent) {
    // 如果在构造/初始化阶段希望抑制节流弹窗则直接允许
    if (suppressThrottle_) return true;

    // 原有逻辑：每 500 ms 最多允许一次
    if (actionTimer_.isValid() && actionTimer_.elapsed() < 500) {
        Logger::instance().warn("AdminWindow: action throttled (too frequent)");
        QMessageBox::warning(parent ? parent : this, "操作过于频繁", "操作过于频繁，请稍后再试（每秒最多2次）。");
        return false;
    }
    actionTimer_.start();
    return true;
}

// 保留原有注释结构
AdminWindow::AdminWindow(Client* client, QWidget* parent)
    : QWidget(parent), client_(client)
{
    // 在构造开始阶段抑制节流提示（避免连续刷新触发警告）
    suppressThrottle_ = true;

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

    // 新增：筛选控件行（名字、价区间、分类、应用/清除）
    {
        QHBoxLayout* filterRow = new QHBoxLayout;
        filterRow->addWidget(new QLabel("名称:"));
        goodsNameFilterEdit = new QLineEdit;
        goodsNameFilterEdit->setPlaceholderText("输入部分名称进行搜索");
        filterRow->addWidget(goodsNameFilterEdit);

        filterRow->addWidget(new QLabel("价格范围:"));
        priceMinSpin = new QDoubleSpinBox;
        priceMinSpin->setRange(0.0, 1e9);
        priceMinSpin->setDecimals(2);
        priceMinSpin->setValue(0.0);
        priceMaxSpin = new QDoubleSpinBox;
        priceMaxSpin->setRange(0.0, 1e9);
        priceMaxSpin->setDecimals(2);
        priceMaxSpin->setValue(1e9);
        filterRow->addWidget(priceMinSpin);
        filterRow->addWidget(new QLabel(" - "));
        filterRow->addWidget(priceMaxSpin);

        filterRow->addWidget(new QLabel("分类:"));
        categoryFilterEdit = new QLineEdit;
        categoryFilterEdit->setPlaceholderText("输入分类（部分匹配）");
        filterRow->addWidget(categoryFilterEdit);

        applyGoodsFilterBtn = new QPushButton("应用筛选");
        clearGoodsFilterBtn = new QPushButton("清除筛选");
        filterRow->addWidget(applyGoodsFilterBtn);
        filterRow->addWidget(clearGoodsFilterBtn);

        goodLayout->addLayout(filterRow);

        // 连接筛选按钮
        connect(applyGoodsFilterBtn, &QPushButton::clicked, this, &AdminWindow::onApplyGoodsFilter);
        connect(clearGoodsFilterBtn, &QPushButton::clicked, this, &AdminWindow::onClearGoodsFilter);
    }

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

    // ---------- 订单筛选行（管理员） ----------
    {
        QHBoxLayout* ordersFilterRow = new QHBoxLayout;
        ordersFilterRow->addWidget(new QLabel("用户手机号:"));
        ordersPhoneFilterEdit = new QLineEdit;
        ordersPhoneFilterEdit->setPlaceholderText("留空查看所有用户订单");
        ordersFilterRow->addWidget(ordersPhoneFilterEdit);

        ordersFilterRow->addWidget(new QLabel("起始时间:"));
        orderStartEdit = new QDateTimeEdit;
        orderStartEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss.zzz");
        orderStartEdit->setCalendarPopup(true);
        // 默认：非常早的时间
        orderStartEdit->setDateTime(QDateTime::fromSecsSinceEpoch(0));
        ordersFilterRow->addWidget(orderStartEdit);

        ordersFilterRow->addWidget(new QLabel("结束时间:"));
        orderEndEdit = new QDateTimeEdit;
        orderEndEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss.zzz");
        orderEndEdit->setCalendarPopup(true);
        orderEndEdit->setDateTime(QDateTime::currentDateTime().addYears(10)); // 远期结束时间默认
        ordersFilterRow->addWidget(orderEndEdit);

        // 状态下拉
        ordersFilterRow->addWidget(new QLabel("状态:"));
        ordersStatusFilter = new QComboBox;
        // 第一项表示全部(-1)
        ordersStatusFilter->addItem("全部", QVariant(-1));
        ordersStatusFilter->addItem("已完成", QVariant(1));
        ordersStatusFilter->addItem("已发货", QVariant(2));
        ordersStatusFilter->addItem("已退货", QVariant(3));
        ordersStatusFilter->addItem("维修中", QVariant(4));
        ordersStatusFilter->addItem("已取消", QVariant(5));
        ordersStatusFilter->addItem("未知", QVariant(0));
        ordersStatusFilter->setCurrentIndex(0);
        ordersFilterRow->addWidget(ordersStatusFilter);

        applyOrdersFilterBtn = new QPushButton("应用筛选");
        clearOrdersFilterBtn = new QPushButton("清除筛选");
        ordersFilterRow->addWidget(applyOrdersFilterBtn);
        ordersFilterRow->addWidget(clearOrdersFilterBtn);

        orderLayout->addLayout(ordersFilterRow);

        connect(applyOrdersFilterBtn, &QPushButton::clicked, this, &AdminWindow::onApplyOrdersFilter);
        connect(clearOrdersFilterBtn, &QPushButton::clicked, this, &AdminWindow::onClearOrdersFilter);
    }
    // ---------- 订单筛选行 结束 ----------

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
    cartPhoneEdit->setPlaceholderText("输入用户手机号(不输入则为拉取所有购物车信息)");
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
    backBtn = new QPushButton("修改主题", this); // 原“切换主题”改为“修改主题”
    returnIdentityBtn = new QPushButton("返回身份选择界面", this);
    bottomBtnLayout->addStretch();
    bottomBtnLayout->addWidget(backBtn);
    bottomBtnLayout->addWidget(returnIdentityBtn);
    mainLayout->addLayout(bottomBtnLayout);

    connect(backBtn, &QPushButton::clicked, this, [this]() {
        Theme::instance().showPresetManager(this);
    });
    connect(returnIdentityBtn, &QPushButton::clicked, this, &AdminWindow::onReturnToIdentitySelection);

    // 连接：切换主题（允许在浅/深之间切换，不再受系统强制）
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        Theme::instance().showPresetManager(this);
        });
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

    // 首次加载 —— 允许批量内部刷新而不触发节流提示
    refreshUsers();
    refreshGoods();
    refreshOrders();
    refreshPromotions();

    // 构造完成后恢复节流行为（用户交互才会被节流）
    suppressThrottle_ = false;

    // 初始化主题：以系统主题为初始值，但用户点击“切换主题”可任意切换
    // 使用 Theme 单例统一管理主题（优先使用持久化设置或基于系统）
    Theme::instance().initFromSystem();
}

void AdminWindow::onReturnToIdentitySelection() {
    Logger::instance().info("AdminWindow::onReturnToIdentitySelection called");
    emit backRequested();
    this->close();
}

// ---------------- 用户/商品/订单 已在之前文件中实现 ----------------
void AdminWindow::refreshUsers() {
    if (!tryThrottle(this)) return;
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
    if (!tryThrottle(this)) return;
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
    if (!tryThrottle(this)) return;
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
    if (!tryThrottle(this)) return;
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
    else QMessageBox::warning(this, "编辑用户", "更新失败（请检查后台或日志）（你是不是没有改？）");
    refreshUsers();
}

void AdminWindow::onDeleteUser() {
    if (!tryThrottle(this)) return;
    Logger::instance().info("AdminWindow::onDeleteUser called");
    if (!client_) return;
    auto items = usersTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "删除用户", "请先选择要删除的用户行");
        return;
    }
    int row = usersTable->row(items.first());
    QString phone = usersTable->item(row, 0)->text();
    QString pwd = usersTable->item(row, 2)->text(); // 管理員界面顯示明文

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

// ---------------- 商品管理 ----------------
//
void AdminWindow::refreshGoodsInternal() {
    goodsTable->setRowCount(0);
    if (!client_) return;
    auto goods = client_->CLTgetAllGoods();

    // 读取筛选条件（本地 UI 操作，不做节流）
    std::string nameFilter;
    double minPrice = 0.0;
    double maxPrice = 1e18;
    std::string categoryFilter;
    if (goodsNameFilterEdit) nameFilter = goodsNameFilterEdit->text().toStdString();
    if (priceMinSpin) minPrice = priceMinSpin->value();
    if (priceMaxSpin) maxPrice = priceMaxSpin->value();
    if (categoryFilterEdit) categoryFilter = categoryFilterEdit->text().toStdString();
    if (maxPrice < minPrice) std::swap(minPrice, maxPrice);

    // 过滤
    std::vector<Good> filtered;
    for (const auto& g : goods) {
        if (!containsIC_admin(g.getName(), nameFilter)) continue;
        if (!containsIC_admin(g.getCategory(), categoryFilter)) continue;
        double p = g.getPrice();
        if (p + 1e-9 < minPrice) continue;
        if (p - 1e-9 > maxPrice) continue;
        filtered.push_back(g);
    }

    goodsTable->setRowCount((int)filtered.size());
    for (int i = 0; i < (int)filtered.size(); ++i) {
        const Good& g = filtered[i];
        goodsTable->setItem(i, 0, new QTableWidgetItem(QString::number(g.getId())));
        goodsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(g.getName())));
        goodsTable->setItem(i, 2, new QTableWidgetItem(QString::number(g.getPrice())));
        goodsTable->setItem(i, 3, new QTableWidgetItem(QString::number(g.getStock())));
        goodsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(g.getCategory())));
    }
}

void AdminWindow::onAddGood() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    bool ok;
    QString name = QInputDialog::getText(this, "新增商品", "名称:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;
    double price = QInputDialog::getDouble(this, "新增商品", "价格（最高支持10,0000.00）:", 0.0, 0, 1e5, 2, &ok);
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
    if (!tryThrottle(this)) return;
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
    double price = QInputDialog::getDouble(this, "编辑商品", "价格（最高支持10,0000.00）:", curPrice, 0, 1e5, 2, &ok);
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
    if (!tryThrottle(this)) return;
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

void AdminWindow::onApplyGoodsFilter() {
    refreshGoodsInternal();
}
void AdminWindow::refreshGoods() {
    if (!tryThrottle(this)) return;
    // 带节流的外部接口调用不再包含具体实现（实现委托给 refreshGoodsInternal）
    refreshGoodsInternal();
}
void AdminWindow::onClearGoodsFilter() {
    // 清除筛选不计入节流（只影响本地控件）
    if (goodsNameFilterEdit) goodsNameFilterEdit->clear();
    if (priceMinSpin) priceMinSpin->setValue(0.0);
    if (priceMaxSpin) priceMaxSpin->setValue(1e9);
    if (categoryFilterEdit) categoryFilterEdit->clear();
    // 直接内部刷新以避免节流弹窗
    refreshGoodsInternal();
}

//
// ---------------- 订单管理 ----------------
void AdminWindow::refreshOrders() {
    if (!tryThrottle(this)) return;
    ordersTable->setRowCount(0);
    if (!client_) return;

    // 管理员可以指定手机号（留空表示获取最近的订单集合）
    QString phoneFilter = (ordersPhoneFilterEdit ? ordersPhoneFilterEdit->text().trimmed() : QString());

    // 请求服务端：若 phoneFilter 为空则获取最近若干订单（服务端实现），否则按用户拉取
    auto orders = client_->CLTgetAllOrders(phoneFilter.toStdString());

    // 读取时间范围
    QDateTime start = orderStartEdit ? orderStartEdit->dateTime() : QDateTime::fromSecsSinceEpoch(0);
    QDateTime end = orderEndEdit ? orderEndEdit->dateTime() : QDateTime::currentDateTime().addYears(10);

    // 读取状态筛选（-1 表示全部）
    int selStatus = -1;
    if (ordersStatusFilter) {
        QVariant v = ordersStatusFilter->currentData();
        if (v.isValid()) selStatus = v.toInt();
    }

    // 同样使用健壮的解析逻辑（注意：避免使用已弃用的 setTimeSpec）
    auto parseOid = [](const std::string& oidStr) -> QDateTime {
        QString oid = QString::fromStdString(oidStr).trimmed();
        if (!oid.startsWith('c')) return QDateTime();
        if (oid.size() < 18) return QDateTime();
        QString sub = oid.mid(1, 17); // YYYYMMDDHHmmsszzz
        QDateTime dt = QDateTime::fromString(sub, "yyyyMMddHHmmsszzz");
        if (dt.isValid()) { dt = dt.toLocalTime(); return dt; }
        QString datePart = sub.left(14);
        QString msPart = sub.mid(14, 3);
        QDateTime dt2 = QDateTime::fromString(datePart, "yyyyMMddHHmmss");
        if (!dt2.isValid()) return QDateTime();
        bool ok = false;
        int msec = msPart.toInt(&ok);
        if (!ok) msec = 0;
        dt2 = dt2.addMSecs(msec);
        dt2 = dt2.toLocalTime();
        return dt2;
    };

    std::vector<Order> filtered;
    filtered.reserve(orders.size());
    for (const auto& o : orders) {
        // 按状态过滤（若选中“全部”即 selStatus == -1 则不过滤）
        if (selStatus != -1 && o.getStatus() != selStatus) continue;

        QDateTime dt = parseOid(o.getOrderId());
        if (dt.isValid()) {
            if (dt >= start && dt <= end) filtered.push_back(o);
        } else {
            // 保留无法解析的订单（如需严格过滤可以改为跳过）
            filtered.push_back(o);
        }
    }

    ordersTable->setRowCount((int)filtered.size());
    for (int i = 0; i < (int)filtered.size(); ++i) {
        const Order& o = filtered[i];
        ordersTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(o.getOrderId())));
        ordersTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(o.getUserPhone())));

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

void AdminWindow::onReturnOrder() {
    if (!tryThrottle(this)) return;
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
    if (!tryThrottle(this)) return;
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
    if (!tryThrottle(this)) return;
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

void AdminWindow::onViewOrderDetail() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    auto items = ordersTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "查看订单", "请先选择订单行"); return; }
    int row = ordersTable->row(items.first());
    QString oid = ordersTable->item(row, 0)->text();
    QString userPhone = ordersTable->item(row, 1) ? ordersTable->item(row, 1)->text() : QString();

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
    }
    else {
        for (const auto& it : itemsVec) {
            txt += QString::fromStdString(it.getGoodName()) + " (id:" + QString::number(it.getGoodId()) + ") x" + QString::number(it.getQuantity())
                + " 单价:" + QString::number(it.getPrice(), 'f', 2) + " 小计:" + QString::number(it.getSubtotal(), 'f', 2) + "\n";
        }
    }
    QMessageBox::information(this, "订单详情", txt);
}


// ---------------- 购物车相关实现（已移除 增/保存） ----------------


void AdminWindow::onLoadCartForUser() {
    if (!tryThrottle(this)) return;
    if (!client_) return;

    QString phone = cartPhoneEdit->text().trimmed();

    // 在开始执行耗时网络操作时禁用按钮，防止重复点击
    loadCartBtn->setEnabled(false);
    struct BtnRestorer { QPushButton* b; BtnRestorer(QPushButton* btn):b(btn){} ~BtnRestorer(){ if(b) b->setEnabled(true);} };
    BtnRestorer restore(loadCartBtn);

    // 若填写了手机号 -> 加载该用户购物车（原有行为）
    if (!phone.isEmpty()) {
        Logger::instance().info(std::string("AdminWindow::onLoadCartForUser: request for userPhone=") + phone.toStdString());
        if (!client_->CLTisConnectionActive()) {
            Logger::instance().warn("AdminWindow::onLoadCartForUser: client not connected, attempting reconnect");
            client_->CLTreconnect();
        }
        TemporaryCart cart = client_->CLTgetCartForUser(phone.toStdString());
        currentCart_ = cart; // cache
        cartTable->setRowCount(0);

        // 保持原来列数与表头（单用户视图）
        cartTable->setColumnCount(5);
        cartTable->setHorizontalHeaderLabels(QStringList() << "good_id" << "名称" << "价格" << "数量" << "小计");

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
        return;
    }

    // phone 为空 -> 加载所有用户的购物车并合并显示（新增）
    Logger::instance().info("AdminWindow::onLoadCartForUser: phone empty -> load all users' carts");
    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("AdminWindow::onLoadCartForUser: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    // 收集所有用户的购物车项（逐用户拉取）
    std::vector<std::tuple<std::string, CartItem>> allItems;
    try {
        auto users = client_->CLTgetAllAccounts();
        for (const auto& u : users) {
            try {
                TemporaryCart cart = client_->CLTgetCartForUser(u.getPhone());
                for (const auto& it : cart.items) {
                    allItems.emplace_back(u.getPhone(), it);
                }
            }
            catch (...) {
                Logger::instance().warn(std::string("AdminWindow::onLoadCartForUser: failed to load cart for ") + u.getPhone());
            }
        }
    }
    catch (...) {
        Logger::instance().warn("AdminWindow::onLoadCartForUser: failed to enumerate accounts");
    }

    cartTable->setRowCount(0);
    if (allItems.empty()) {
        QMessageBox::information(this, "加载购物车", "未找到任何购物车或所有购物车均为空");
        // 仍然清空并设置为默认列
        cartTable->setColumnCount(5);
        cartTable->setHorizontalHeaderLabels(QStringList() << "good_id" << "名称" << "价格" << "数量" << "小计");
        return;
    }

    // 多用户视图：增加第一列显示用户手机号（便于区分与后续操作）
    cartTable->setColumnCount(6);
    cartTable->setHorizontalHeaderLabels(QStringList() << "用户手机号" << "good_id" << "名称" << "价格" << "数量" << "小计");
    cartTable->setRowCount(static_cast<int>(allItems.size()));
    for (int i = 0; i < (int)allItems.size(); ++i) {
        const std::string& userPhone = std::get<0>(allItems[i]);
        const CartItem& it = std::get<1>(allItems[i]);
        QTableWidgetItem* userItem = new QTableWidgetItem(QString::fromStdString(userPhone));
        QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(it.good_id));
        // 把 userPhone 存到 idItem 的 UserRole 中，方便编辑/删除时读取对应用户
        idItem->setData(Qt::UserRole, QString::fromStdString(userPhone));
        cartTable->setItem(i, 0, userItem);
        cartTable->setItem(i, 1, idItem);
        cartTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(it.good_name)));
        cartTable->setItem(i, 3, new QTableWidgetItem(QString::number(it.price)));
        cartTable->setItem(i, 4, new QTableWidgetItem(QString::number(it.quantity)));
        cartTable->setItem(i, 5, new QTableWidgetItem(QString::number(it.subtotal)));
    }
    Logger::instance().info(std::string("AdminWindow::onLoadCartForUser: loaded all carts total items=") + std::to_string(allItems.size()));
}

void AdminWindow::onEditCartItem() {
    if (!tryThrottle(this)) return;
    if (!client_) return;

    // 首先尝试从输入框读取手机号；若为空则从表格行的 UserRole 中读取（多用户视图）
    QString phone = cartPhoneEdit->text().trimmed();

    auto items = cartTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "修改数量", "请先选择要修改的商品行");
        return;
    }
    int row = cartTable->row(items.first());
    if (row < 0) {
        QMessageBox::warning(this, "修改数量", "无法确定选中行");
        return;
    }

    // 计算列索引（支持单用户/多用户两种表格布局）
    int colCount = cartTable->columnCount();
    int productIdCol = (colCount == 5) ? 0 : 1;
    int qtyCol = (colCount == 5) ? 3 : 4;

    // 若 phone 为空且表格存储有 UserRole 则用之
    if (phone.isEmpty()) {
        QTableWidgetItem* idItemRole = cartTable->item(row, productIdCol);
        if (idItemRole && idItemRole->data(Qt::UserRole).isValid()) {
            phone = idItemRole->data(Qt::UserRole).toString().trimmed();
        }
    }

    if (phone.isEmpty()) {
        QMessageBox::warning(this, "修改数量", "请输入手机号或在表格中选择带手机号的条目以修改");
        return;
    }

    // 读取 productId 与当前数量
    QTableWidgetItem* idItem = cartTable->item(row, productIdCol);
    QTableWidgetItem* qtyItem = cartTable->item(row, qtyCol);
    if (!idItem || !qtyItem) {
        QMessageBox::warning(this, "修改数量", "读取选中行数据失败（请确保整行被选中）");
        return;
    }

    int productId = idItem->text().toInt();
    int currentQty = qtyItem->text().toInt();

    bool ok;
    int newQty = QInputDialog::getInt(this, "修改数量", "新数量:", currentQty, 0, 1000000, 1, &ok);
    if (!ok) return;

    // 若 newQty == 0 当作删除
    if (newQty == 0) {
        if (QMessageBox::question(this, "删除商品", "数量设为 0，是否从购物车删除该商品？") != QMessageBox::Yes) {
            return;
        }
        if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
        bool removed = client_->CLTremoveFromCart(phone.toStdString(), productId);
        Logger::instance().info(std::string("AdminWindow::onEditCartItem -> remove CLTremoveFromCart returned ") + (removed ? "true" : "false"));
        if (removed) {
            // 强制切换到 id:0 原价占位策略
            auto toIdString = [](const nlohmann::json& j) -> QString {
                try {
                    if (j.contains("id")) {
                        if (j["id"].is_string()) return QString::fromStdString(j["id"].get<std::string>());
                        if (j["id"].is_number_integer()) return QString::number(j["id"].get<long long>());
                        if (j["id"].is_number_unsigned()) return QString::number(j["id"].get<unsigned long long>());
                        if (j["id"].is_number_float()) return QString::number(j["id"].get<double>(), 'f', 0);
                    }
                }
                catch (...) {}
                return QString();
                };
            try {
                QString id0Display, id0PolicyStr;
                auto raws = client_->CLTgetAllPromotionsRaw();
                for (const auto& r : raws) {
                    if (toIdString(r) == "0") {
                        std::string n0 = r.value("name", std::string(""));
                        std::string policyStr;
                        try {
                            if (r.contains("policy")) policyStr = r["policy"].dump();
                            else policyStr = r.value("policy_detail", std::string(""));
                        }
                        catch (...) { policyStr = r.value("policy_detail", std::string("")); }
                        id0Display = n0.empty() ? "原价(占位)" : QString::fromStdString(n0);
                        id0PolicyStr = QString::fromStdString(policyStr);
                        break;
                    }
                }

                if (!id0PolicyStr.isEmpty()) {
                    TemporaryCart cart = client_->CLTgetCartForUser(phone.toStdString());
                    double original = 0.0;
                    for (const auto& it : cart.items) original += it.price * it.quantity;

                    cart.discount_policy = id0Display.toStdString();
                    if (cart.total_amount <= 0.0) cart.total_amount = original;
                    cart.final_amount = original;
                    cart.discount_amount = 0.0;

                    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
                    bool saved = client_->CLTsaveCartForUserWithPolicy(cart, id0PolicyStr.toStdString());
                    Logger::instance().info(std::string("AdminWindow::onEditCartItem(remove): enforce id:0 policy -> ") + (saved ? "ok" : "fail"));
                }
                else {
                    Logger::instance().warn("AdminWindow::onEditCartItem(remove): id:0 promotion not found");
                }
            }
            catch (...) {
                Logger::instance().warn("AdminWindow::onEditCartItem(remove): exception enforcing id:0 policy");
            }

            QMessageBox::information(this, "删除商品", QString("已从 %1 的购物车删除").arg(phone));
        }
        else {
            QMessageBox::warning(this, "删除商品", "删除失败（查看日志）（很有可能是超过库存了）");
        }
        onLoadCartForUser();
        return;
    }

    // 更新数量（重试一次）
    int attempts = 0;
    bool updated = false;
    for (; attempts < 2; ++attempts) {
        if (!client_->CLTisConnectionActive()) {
            Logger::instance().warn("AdminWindow::onEditCartItem: client not connected, attempting reconnect");
            bool rc = client_->CLTreconnect();
            Logger::instance().info(std::string("AdminWindow::onEditCartItem: CLTreconnect returned ") + (rc ? "true" : "false"));
        }
        Logger::instance().info(std::string("AdminWindow::onEditCartItem: attempt update productId=") + std::to_string(productId) +
            ", newQty=" + std::to_string(newQty) + ", user=" + phone.toStdString() + ", attempt=" + std::to_string(attempts + 1));
        updated = client_->CLTupdateCartItem(phone.toStdString(), productId, newQty);
        Logger::instance().info(std::string("AdminWindow::onEditCartItem CLTupdateCartItem returned ") + (updated ? "true" : "false"));
        if (updated) break;
    }

    if (updated) {
        // 强制切换到 id:0 原价占位策略
        auto toIdString = [](const nlohmann::json& j) -> QString {
            try {
                if (j.contains("id")) {
                    if (j["id"].is_string()) return QString::fromStdString(j["id"].get<std::string>());
                    if (j["id"].is_number_integer()) return QString::number(j["id"].get<long long>());
                    if (j["id"].is_number_unsigned()) return QString::number(j["id"].get<unsigned long long>());
                    if (j["id"].is_number_float()) return QString::number(j["id"].get<double>(), 'f', 0);
                }
            }
            catch (...) {}
            return QString();
            };
        try {
            TemporaryCart cart = client_->CLTgetCartForUser(phone.toStdString());
            double original = 0.0;
            for (const auto& it : cart.items) original += it.price * it.quantity;

            QString id0Display, id0PolicyStr;
            auto raws = client_->CLTgetAllPromotionsRaw();
            for (const auto& r : raws) {
                if (toIdString(r) == "0") {
                    std::string name0 = r.value("name", std::string(""));
                    std::string policyStr;
                    try {
                        if (r.contains("policy")) policyStr = r["policy"].dump();
                        else policyStr = r.value("policy_detail", std::string(""));
                    }
                    catch (...) { policyStr = r.value("policy_detail", std::string("")); }
                    id0Display = name0.empty() ? "原价(占位)" : QString::fromStdString(name0);
                    id0PolicyStr = QString::fromStdString(policyStr);
                    break;
                }
            }

            if (!id0PolicyStr.isEmpty()) {
                cart.discount_policy = id0Display.toStdString();
                cart.total_amount = original;
                cart.final_amount = original;
                cart.discount_amount = 0.0;
                if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
                bool saved = client_->CLTsaveCartForUserWithPolicy(cart, id0PolicyStr.toStdString());
                Logger::instance().info(std::string("AdminWindow::onEditCartItem(update): enforce id:0 policy -> ") + (saved ? "ok" : "fail"));
            }
            else {
                Logger::instance().warn("AdminWindow::onEditCartItem(update): id:0 promotion not found");
            }
        }
        catch (...) {
            Logger::instance().warn("AdminWindow::onEditCartItem(update): exception enforcing id:0 policy");
        }

        QMessageBox::information(this, "修改数量", "已更新数量");
    }
    else {
        QMessageBox::warning(this, "修改数量", "更新失败（查看日志）(很有可能是超过库存了)");
    }

    onLoadCartForUser();
}

void AdminWindow::onRemoveCartItem() {
    if (!tryThrottle(this)) return;
    if (!client_) return;

    auto items = cartTable->selectedItems();
    if (items.empty()) {
        QMessageBox::warning(this, "删除条目", "请先选择要删除的商品行");
        return;
    }
    int row = cartTable->row(items.first());
    if (row < 0) {
        QMessageBox::warning(this, "删除条目", "无法确定选中行");
        return;
    }

    // 尝试获取手机号：优先使用输入框，否则从表格 UserRole 中读取
    QString phone = cartPhoneEdit->text().trimmed();
    int colCount = cartTable->columnCount();
    int productIdCol = (colCount == 5) ? 0 : 1;

    if (phone.isEmpty()) {
        QTableWidgetItem* idItemRole = cartTable->item(row, productIdCol);
        if (idItemRole && idItemRole->data(Qt::UserRole).isValid()) {
            phone = idItemRole->data(Qt::UserRole).toString().trimmed();
        }
        else if (colCount >= 6) {
            QTableWidgetItem* userIt = cartTable->item(row, 0);
            if (userIt) phone = userIt->text().trimmed();
        }
    }

    if (phone.isEmpty()) {
        QMessageBox::warning(this, "删除条目", "请输入手机号或选择包含手机号的条目");
        return;
    }

    QTableWidgetItem* idItem = cartTable->item(row, productIdCol);
    if (!idItem) {
        QMessageBox::warning(this, "删除条目", "读取选中行失败");
        return;
    }
    int productId = idItem->text().toInt();

    if (QMessageBox::question(this, "删除条目", QString("确认从 %1 的购物车删除商品 ID=%2 ?").arg(phone).arg(productId)) != QMessageBox::Yes) {
        return;
    }

    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    bool res = client_->CLTremoveFromCart(phone.toStdString(), productId);
    Logger::instance().info(std::string("AdminWindow::onRemoveCartItem CLTremoveFromCart returned ") + (res ? "true" : "false"));
    if (!res) {
        QMessageBox::warning(this, "删除条目", "删除失败（检查日志）");
        return;
    }

    // 强制切换到 id:0 原价占位策略
    auto toIdString = [](const nlohmann::json& j) -> QString {
        try {
            if (j.contains("id")) {
                if (j["id"].is_string()) return QString::fromStdString(j["id"].get<std::string>());
                if (j["id"].is_number_integer()) return QString::number(j["id"].get<long long>());
                if (j["id"].is_number_unsigned()) return QString::number(j["id"].get<unsigned long long>());
                if (j["id"].is_number_float()) return QString::number(j["id"].get<double>(), 'f', 0);
            }
        }
        catch (...) {}
        return QString();
        };

    try {
        QString id0Display, id0PolicyStr;
        auto raws = client_->CLTgetAllPromotionsRaw();
        for (const auto& r : raws) {
            if (toIdString(r) == "0") {
                std::string n0 = r.value("name", std::string(""));
                std::string policyStr;
                try {
                    if (r.contains("policy")) policyStr = r["policy"].dump();
                    else policyStr = r.value("policy_detail", std::string(""));
                }
                catch (...) { policyStr = r.value("policy_detail", std::string("")); }
                id0Display = n0.empty() ? "原价(占位)" : QString::fromStdString(n0);
                id0PolicyStr = QString::fromStdString(policyStr);
                break;
            }
        }

        if (!id0PolicyStr.isEmpty()) {
            TemporaryCart cart = client_->CLTgetCartForUser(phone.toStdString());
            double original = 0.0;
            for (const auto& it : cart.items) original += it.price * it.quantity;

            cart.discount_policy = id0Display.toStdString();
            if (cart.total_amount <= 0.0) cart.total_amount = original;
            cart.final_amount = original;
            cart.discount_amount = 0.0;

            if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
            bool saved = client_->CLTsaveCartForUserWithPolicy(cart, id0PolicyStr.toStdString());
            Logger::instance().info(std::string("AdminWindow::onRemoveCartItem: enforce id:0 policy -> ") + (saved ? "ok" : "fail"));
        }
        else {
            Logger::instance().warn("AdminWindow::onRemoveCartItem: id:0 promotion not found");
        }
    }
    catch (...) {
        Logger::instance().warn("AdminWindow::onRemoveCartItem: exception enforcing id:0 policy");
    }

    QMessageBox::information(this, "删除条目", "删除成功，正在刷新购物车");
    onLoadCartForUser();
}

// ------------------- 促销相关实现 -------------------
// 在构造函数中创建 promotions tab（示例放在 AdminWindow 构造尾部或合适位置）
// 在 createPromotionsTab 中，加入“类型”筛选下拉并连接变化事件到 refreshPromotions()
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
    // 禁止表格内联编辑
    promoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QHBoxLayout* btnRow = new QHBoxLayout;

    // 新增：类型筛选
    btnRow->addWidget(new QLabel("类型:"));
    promosTypeFilter = new QComboBox(promoTab);
    promosTypeFilter->addItem("全部", QVariant("ALL"));
    promosTypeFilter->addItem("统一折扣(discount)", QVariant("discount"));
    promosTypeFilter->addItem("阶梯折扣(tiered)", QVariant("tiered"));
    promosTypeFilter->addItem("满减(full_reduction)", QVariant("full_reduction"));
    promosTypeFilter->addItem("未知/其他", QVariant("UNKNOWN"));
    promosTypeFilter->setCurrentIndex(0);
    btnRow->addWidget(promosTypeFilter);

    refreshPromosBtn = new QPushButton("刷新", promoTab);
    addPromoBtn = new QPushButton("新增", promoTab);
    QPushButton* viewPromoDetailBtn = new QPushButton("查看详情", promoTab); // 新增：查看详情
    deletePromoBtn = new QPushButton("删除", promoTab);

    btnRow->addWidget(refreshPromosBtn);
    btnRow->addWidget(addPromoBtn);
    btnRow->addWidget(viewPromoDetailBtn); // 新增按钮
    btnRow->addWidget(deletePromoBtn);
    btnRow->addStretch();

    v->addLayout(btnRow);
    v->addWidget(promoTable);
    tabWidget->addTab(promoTab, "促销");

    // 连接信号
    connect(refreshPromosBtn, &QPushButton::clicked, this, &AdminWindow::refreshPromotions);
    connect(deletePromoBtn, &QPushButton::clicked, this, &AdminWindow::onDeletePromotion);
    connect(addPromoBtn, &QPushButton::clicked, this, &AdminWindow::onAddPromotion);
    connect(viewPromoDetailBtn, &QPushButton::clicked, this, &AdminWindow::onViewPromotionDetail);

    // 类型变化即刷新
    connect(promosTypeFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        this->refreshPromotions();
        });

    // 双击查看详情（不进入编辑）
    connect(promoTable, &QTableWidget::cellDoubleClicked, this, [this](int /*row*/, int /*col*/) {
        this->onViewPromotionDetail();
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
    if (!tryThrottle(this)) return;
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
    // 新增：管理员禁用在窗口直接修改促销
    QMessageBox::information(this, "编辑促销", "已禁用在窗口直接修改促销策略。");
    return;
    //史山来咯
    // 保留原实现（不会被执行，仅为兼容编译与后续可能的策略调整）
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
// 在 refreshPromotions 中依据下拉选择过滤
void AdminWindow::refreshPromotions() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    promoTable->setRowCount(0);

    QString selType = "ALL";
    if (promosTypeFilter) {
        QVariant v = promosTypeFilter->currentData();
        if (v.isValid()) selType = v.toString();
    }

    auto rows = client_->CLTgetAllPromotionsRaw();
    int outRow = 0;
    promoTable->setRowCount(static_cast<int>(rows.size())); // 先设最大，再按需填
    for (const auto& r : rows) {
        QString id = QString::fromStdString(r.value("id", std::string("")));
        QString name = QString::fromStdString(r.value("name", std::string("")));

        QString type;
        try {
            if (r.contains("policy") && r["policy"].is_object()) {
                type = QString::fromStdString(r["policy"].value("type", std::string("")));
            }
            else {
                type = QString::fromStdString(r.value("type", std::string("")));
            }
        }
        catch (...) {
            type = QString::fromStdString(r.value("type", std::string("")));
        }

        // 依据选择过滤
        bool isUnknown = type.trimmed().isEmpty();
        if (selType != "ALL") {
            if (selType == "UNKNOWN") {
                if (!isUnknown) continue;
            }
            else {
                if (type != selType) continue;
            }
        }

        QString policyStr;
        try {
            if (r.contains("policy")) policyStr = QString::fromStdString(r["policy"].dump());
            else policyStr = QString::fromStdString(r.value("policy_detail", std::string("")));
        }
        catch (...) {
            policyStr = QString::fromStdString(r.value("policy_detail", std::string("")));
        }

        QTableWidgetItem* idItem = new QTableWidgetItem(id);
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        QTableWidgetItem* typeItem = new QTableWidgetItem(type);
        QTableWidgetItem* policyItem = new QTableWidgetItem(policyStr.left(200));
        policyItem->setData(Qt::UserRole, policyStr); // 保存完整 policy

        promoTable->setItem(outRow, 0, idItem);
        promoTable->setItem(outRow, 1, nameItem);
        promoTable->setItem(outRow, 2, typeItem);
        promoTable->setItem(outRow, 3, policyItem);
        ++outRow;
    }
    promoTable->setRowCount(outRow); // 收缩到真实行数
    Logger::instance().info(std::string("AdminWindow::refreshPromotions loaded (filtered) ") + std::to_string(outRow) + " promotions");
}

void AdminWindow::onDeletePromotion() {
    if (!tryThrottle(this)) return;
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
    QString nameQ = promoTable->item(row, 1)->text().trimmed();

    // 提取本行的 id 文本（若列中为空，后续从服务端补查）
    QString idText = promoTable->item(row, 0) ? promoTable->item(row, 0)->text().trimmed() : QString();

    // 辅助：健壮读取 json 中的 id 为字符串
    auto toIdString = [](const nlohmann::json& j) -> QString {
        try {
            if (j.contains("id")) {
                if (j["id"].is_string()) return QString::fromStdString(j["id"].get<std::string>());
                if (j["id"].is_number_integer()) return QString::number(j["id"].get<long long>());
                if (j["id"].is_number_unsigned()) return QString::number(j["id"].get<unsigned long long>());
                if (j["id"].is_number_float()) return QString::number(j["id"].get<double>(), 'f', 0);
            }
        }
        catch (...) {}
        return QString();
        };

    // 若表格 id 为空或不可靠，通过名称到服务端查一次，取权威 id
    if (idText.isEmpty()) {
        try {
            auto raws = client_->CLTgetAllPromotionsRaw();
            for (const auto& r : raws) {
                QString nm = QString::fromStdString(r.value("name", std::string("")));
                if (nm == nameQ) {
                    idText = toIdString(r).trimmed();
                    break;
                }
            }
        }
        catch (...) {}
    }

    // 保护：默认占位促销（id:0）禁止删除
    if (idText == "0") {
        QMessageBox::information(this, "删除促销", "已禁止删除默认促销（id:0）。");
        return;
    }

    if (QMessageBox::question(this, "删除促销",
        QString("确定要删除促销 “%1” 吗？").arg(nameQ)) != QMessageBox::Yes) return;

    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("AdminWindow::onDeletePromotion: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    bool ok = client_->CLTdeletePromotion(nameQ.toStdString());
    QMessageBox::information(this, "删除促销", ok ? "删除成功" : "删除失败（查看日志）");
    refreshPromotions();
}

// 新增：筛选按钮槽实现（加入到 AdminWindow.cpp 中合适位置）
void AdminWindow::onApplyOrdersFilter() {
    // 直接刷新订单以应用当前筛选控件的值
    refreshOrders();
}

void AdminWindow::onClearOrdersFilter() {
    if (ordersPhoneFilterEdit) ordersPhoneFilterEdit->clear();
    if (orderStartEdit) orderStartEdit->setDateTime(QDateTime::fromSecsSinceEpoch(0));
    if (orderEndEdit) orderEndEdit->setDateTime(QDateTime::currentDateTime().addYears(10));
    if (ordersStatusFilter) ordersStatusFilter->setCurrentIndex(0);
    refreshOrders();
}

void AdminWindow::onViewPromotionDetail() {
    auto sel = promoTable->selectionModel()->selectedRows();
    if (sel.empty()) {
        QMessageBox::information(this, "促销详情", "请先选择一条促销。");
        return;
    }
    int row = sel.first().row();
    QString id = promoTable->item(row, 0) ? promoTable->item(row, 0)->text() : "";
    QString name = promoTable->item(row, 1) ? promoTable->item(row, 1)->text() : "";
    QString type = promoTable->item(row, 2) ? promoTable->item(row, 2)->text() : "";
    QString policyFull = promoTable->item(row, 3) ? promoTable->item(row, 3)->data(Qt::UserRole).toString() : "";

    // 构造人类可读摘要
    QString summary = "(未知策略)";
    try {
        nlohmann::json js = policyFull.isEmpty() ? nlohmann::json() : nlohmann::json::parse(policyFull.toStdString());
        if (js.is_object()) {
            std::string t = js.value("type", std::string(""));
            if (t == "discount") {
                double d = js.value("discount", 1.0);
                summary = QString("类型: 统一折扣\n折扣率: %1 (约 %2 折)").arg(d, 0, 'f', 4).arg(d * 10.0, 0, 'f', 2);
            }
            else if (t == "full_reduction" || t == "fullReduction") {
                double thr = js.value("threshold", 0.0);
                double reduce = js.value("reduce", 0.0);
                summary = QString("类型: 满减\n满: %1  减: %2").arg(thr, 0, 'f', 2).arg(reduce, 0, 'f', 2);
            }
            else if (t == "tiered") {
                QString lines = "类型: 阶梯折扣\n档位:\n";
                if (js.contains("tiers") && js["tiers"].is_array()) {
                    for (const auto& tier : js["tiers"]) {
                        int minq = tier.value("min_qty", 0);
                        double disc = tier.value("discount", 1.0);
                        lines += QString(" - min_qty=%1, discount=%2 (约 %3 折)\n")
                            .arg(minq).arg(disc, 0, 'f', 4).arg(disc * 10.0, 0, 'f', 2);
                    }
                }
                summary = lines.trimmed();
            }
            else {
                summary = "类型: 未知/其他";
            }
        }
        else if (!policyFull.isEmpty()) {
            summary = "策略文本（policy_detail）";
        }
    }
    catch (...) {
        summary = "策略文本（policy_detail）";
    }

    // 生成 pretty JSON 文本
    QString pretty;
    try {
        if (!policyFull.isEmpty()) {
            nlohmann::json js = nlohmann::json::parse(policyFull.toStdString());
            pretty = QString::fromStdString(js.dump(2));
        }
    }
    catch (...) {
        pretty = policyFull;
    }

    // 弹出详情对话框（可复制）
    QDialog dlg(this);
    dlg.setWindowTitle("促销详情");
    dlg.resize(640, 480);
    QVBoxLayout* lay = new QVBoxLayout(&dlg);
    QLabel* header = new QLabel(QString("ID: %1    名称: %2\n类型: %3").arg(id, name, type));
    header->setWordWrap(true);
    lay->addWidget(header);
    QLabel* sumLab = new QLabel(summary);
    sumLab->setWordWrap(true);
    lay->addWidget(sumLab);
    QTextEdit* text = new QTextEdit(pretty);
    text->setReadOnly(true);
    lay->addWidget(text, 1);
    QHBoxLayout* br = new QHBoxLayout;
    QPushButton* closeBtn = new QPushButton("关闭");
    br->addStretch(); br->addWidget(closeBtn);
    lay->addLayout(br);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}