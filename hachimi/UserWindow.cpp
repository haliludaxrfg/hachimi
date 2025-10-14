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
#include <nlohmann/json.hpp>
#include <QComboBox>
#include <algorithm>
#include <QDialog>
#include <thread>
#include <chrono>
#include <QApplication> // 用于切换主题
#include <QDoubleSpinBox> // 新增：价格筛选控件
#include <QDateTimeEdit> // 新增：日期时间筛选控件
#include <unordered_set>
#include "Theme.h"

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

// 判断系统是否为深色主题（基于 QPalette::Window 的 lightness）
static bool isSystemDarkTheme_UserWindow() {
    QColor bg = qApp->palette().color(QPalette::Window);
    return bg.lightness() < 128;
}

// 主题切换支持：应用简单的暗黑样式或恢复默认（浅色）
// 初始值在构造函数中根据系统主题设置，用户可任意切换
static bool s_darkMode_UserWindow = false;
static void applyTheme_UserWindow(bool dark) {
    if (dark) {
        QString ss = R"(
            /* 深色主题：深海背景 + 柔和青绿色强调（与 AdminWindow 保持一致） */
            QWidget { background-color: #0B1A1E; color: #E6F1F2; }

            QTabWidget::pane { background: transparent; }
            QTabBar::tab { background: transparent; padding: 6px 12px; color: #CFEFEA; }
            QTabBar::tab:selected { background: rgba(45,212,191,0.10); border-radius:4px; }

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

            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #175656, stop:1 #0F3B3E);
                color: #F8FFFE;
                border: 1px solid #1E6E69;
                border-radius: 4px;
                padding: 6px 10px;
            }
            QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1f7f78, stop:1 #126c68); }
            QPushButton:pressed { background-color: #0b5b55; }

            QMessageBox { background-color: #071014; color: #E6F1F2; }
            QMenu { background-color: #071014; color: #E6F1F2; }
            QToolTip { background-color: #0f2a2e; color: #E6F1F2; border: 1px solid #1E3A3E; }
        )";
        qApp->setStyleSheet(ss);
    }
    else {
        qApp->setStyleSheet(QString());
    }
    s_darkMode_UserWindow = dark;
    Logger::instance().info(std::string("UserWindow: theme switched to ") + (dark ? "dark" : "light"));
}

// helper: case-insensitive substring
static bool containsIC(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = haystack;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// 简单的价格展示对话框（复用在显示原价/折后价）
class PriceWindow : public QDialog {
public:
    PriceWindow(const QString &title, double amount, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(title);
        QVBoxLayout* l = new QVBoxLayout(this);
        QLabel* label = new QLabel(QString::number(amount, 'f', 2), this);
        label->setAlignment(Qt::AlignCenter);
        QFont f = label->font();
        f.setPointSize(14);
        f.setBold(true);
        label->setFont(f);
        l->addWidget(label);
        QPushButton* closeBtn = new QPushButton("关闭", this);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        l->addWidget(closeBtn, 0, Qt::AlignCenter);
        setLayout(l);
        setFixedSize(300, 150);
    }
};

// 节流实现：每秒最多允许一次（首次允许）
bool UserWindow::tryThrottle(QWidget* parent) {
    if (actionTimer_.isValid() && actionTimer_.elapsed() < 1000) {
        Logger::instance().warn("UserWindow: action throttled (too frequent)");
        QMessageBox::warning(parent ? parent : this, "操作过于频繁", "操作过于频繁，请稍后再试（每秒最多一次）。");
        return false;
    }
    actionTimer_.start();
    return true;
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

        goodsLayout->addLayout(filterRow);

        // 连接筛选按钮
        connect(applyGoodsFilterBtn, &QPushButton::clicked, this, &UserWindow::onApplyGoodsFilter);
        connect(clearGoodsFilterBtn, &QPushButton::clicked, this, &UserWindow::onClearGoodsFilter);
    }

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
    showOriginalBtn = new QPushButton("查看原价总额");
    showDiscountedBtn = new QPushButton("查看折后总额");
    cartBtnLayout->addWidget(refreshCartBtn);
    cartBtnLayout->addWidget(modifyCartBtn);
    cartBtnLayout->addWidget(removeCartBtn);
    cartBtnLayout->addWidget(checkoutBtn);
    cartBtnLayout->addWidget(showOriginalBtn);
    cartBtnLayout->addWidget(showDiscountedBtn);
    goodsLayout->addLayout(cartBtnLayout);

    // 创建促销控件并连接（放到购物车区域下面）
    promoCombo = new QComboBox();
    promoCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    applyPromoBtn = new QPushButton("应用促销");
    clearPromoBtn = new QPushButton("清除促销");
    {
        QHBoxLayout* promoRow = new QHBoxLayout;
        promoRow->addWidget(new QLabel("可用促销:"));
        promoRow->addWidget(promoCombo);
        promoRow->addWidget(applyPromoBtn);
        promoRow->addWidget(clearPromoBtn);
        QWidget* promoWidget = new QWidget;
        promoWidget->setLayout(promoRow);
        // 将促销控件放在购物车区域下面（在 cart 部分添加之后会显示在下方）
        goodsLayout->addWidget(promoWidget);
    }
    connect(applyPromoBtn, &QPushButton::clicked, this, &UserWindow::onApplyPromotion);
    connect(clearPromoBtn, &QPushButton::clicked, this, &UserWindow::onClearPromotion);
    // 初次加载促销
    refreshPromotions();

    tabWidget->addTab(goodsTab, "商品与购物车");

    // 3. 订单查看
    orderTab = new QWidget;
    QVBoxLayout* orderLayout = new QVBoxLayout(orderTab);
    orderTable = new QTableWidget;
    // 改为 4 列：订单ID / 状态 / 实付金额 / 地址
    orderTable->setColumnCount(4);
    orderTable->setHorizontalHeaderLabels(QStringList() << "订单ID" << "状态" << "实付金额" << "地址");
    orderTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 强制整行选择、单选并禁止编辑，避免用户只选单元格导致读取错误（与购物车行为一致）
    orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    orderTable->setSelectionMode(QAbstractItemView::SingleSelection);
    orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    orderLayout->addWidget(orderTable);

    // 按钮行：刷新/查看详情/退货/维修/删除（已连接到各自槽，槽内实现包含节流）
    {
        QHBoxLayout* orderBtnLayout = new QHBoxLayout;
        refreshOrdersBtn = new QPushButton("刷新订单");
        viewOrderBtn = new QPushButton("查看订单详情");
        returnOrderBtn = new QPushButton("退货");
        repairOrderBtn = new QPushButton("维修");
        deleteOrderBtn = new QPushButton("删除订单");

        orderBtnLayout->addWidget(refreshOrdersBtn);
        orderBtnLayout->addWidget(viewOrderBtn);
        orderBtnLayout->addWidget(returnOrderBtn);
        orderBtnLayout->addWidget(repairOrderBtn);
        orderBtnLayout->addWidget(deleteOrderBtn);

        orderLayout->addLayout(orderBtnLayout);
    }

    // ---------- 订单筛选（用户视图：时间区间） ----------
    {
        QHBoxLayout* orderFilterRow = new QHBoxLayout;
        orderFilterRow->addWidget(new QLabel("起始时间:"));
        orderStartEdit = new QDateTimeEdit;
        orderStartEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss.zzz");
        orderStartEdit->setCalendarPopup(true);
        orderStartEdit->setDateTime(QDateTime::fromSecsSinceEpoch(0));
        orderFilterRow->addWidget(orderStartEdit);

        orderFilterRow->addWidget(new QLabel("结束时间:"));
        orderEndEdit = new QDateTimeEdit;
        orderEndEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss.zzz");
        orderEndEdit->setCalendarPopup(true);
        orderEndEdit->setDateTime(QDateTime::currentDateTime().addYears(10));
        orderFilterRow->addWidget(orderEndEdit);


        orderFilterRow->addWidget(new QLabel("状态:"));
        orderStatusFilter = new QComboBox;
        orderStatusFilter->addItem("全部", QVariant(-1));
        orderStatusFilter->addItem("已完成", QVariant(1));
        orderStatusFilter->addItem("已发货", QVariant(2));
        orderStatusFilter->addItem("已退货", QVariant(3));
        orderStatusFilter->addItem("维修中", QVariant(4));
        orderStatusFilter->addItem("已取消", QVariant(5));
        orderStatusFilter->addItem("未知", QVariant(0));
        orderStatusFilter->setCurrentIndex(0);
        orderFilterRow->addWidget(orderStatusFilter);

        applyOrdersFilterBtn = new QPushButton("应用筛选");
        clearOrdersFilterBtn = new QPushButton("清除筛选");
        orderFilterRow->addWidget(applyOrdersFilterBtn);
        orderFilterRow->addWidget(clearOrdersFilterBtn);

        orderLayout->addLayout(orderFilterRow);

        connect(applyOrdersFilterBtn, &QPushButton::clicked, this, &UserWindow::onApplyOrdersFilter);
        connect(clearOrdersFilterBtn, &QPushButton::clicked, this, &UserWindow::onClearOrdersFilter);
    }
    // ---------- 订单筛选结束 ----------

    tabWidget->addTab(orderTab, "我的订单");

    mainLayout->addWidget(tabWidget);

    // 返回按钮及“返回身份选择界面”按钮，横向排列
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout;
    backBtn = new QPushButton("切换主题", this); // 改为切换主题
    returnIdentityBtn = new QPushButton("返回身份选择界面", this);
    bottomBtnLayout->addStretch();
    bottomBtnLayout->addWidget(backBtn);
    bottomBtnLayout->addWidget(returnIdentityBtn);
    mainLayout->addLayout(bottomBtnLayout);

    // 连接槽：切换主题（替代原来的 close），允许在浅/深之间切换（不受系统强制）
    connect(backBtn, &QPushButton::clicked, this, []() {
        Theme::instance().toggle();
    });
    connect(returnIdentityBtn, &QPushButton::clicked, this, &UserWindow::onReturnToIdentitySelection);

    connect(refreshGoodsBtn, &QPushButton::clicked, this, &UserWindow::refreshGoods);
    connect(addToCartBtn, &QPushButton::clicked, this, &UserWindow::onAddToCart);
    connect(refreshCartBtn, &QPushButton::clicked, this, &UserWindow::refreshCart);
    connect(modifyCartBtn, &QPushButton::clicked, this, &UserWindow::onModifyCartItem);
    connect(removeCartBtn, &QPushButton::clicked, this, &UserWindow::onRemoveCartItem);
    connect(checkoutBtn, &QPushButton::clicked, this, &UserWindow::onCheckout);
    connect(showOriginalBtn, &QPushButton::clicked, this, &UserWindow::onShowOriginalTotal);
    connect(showDiscountedBtn, &QPushButton::clicked, this, &UserWindow::onShowDiscountedTotal);

    // 新增连接：保存与注销
    connect(saveInfoBtn, &QPushButton::clicked, this, &UserWindow::onSaveUserInfo);
    connect(deleteAccountBtn, &QPushButton::clicked, this, &UserWindow::onDeleteAccount);

    connect(refreshOrdersBtn, &QPushButton::clicked, this, &UserWindow::refreshOrders);
    connect(viewOrderBtn, &QPushButton::clicked, this, &UserWindow::onViewOrderDetail);
    connect(returnOrderBtn, &QPushButton::clicked, this, &UserWindow::onReturnOrder);
    connect(repairOrderBtn, &QPushButton::clicked, this, &UserWindow::onRepairOrder);
    connect(deleteOrderBtn, &QPushButton::clicked, this, &UserWindow::onDeleteOrder);

    // 强制整行选择、单选并禁止编辑，避免用户只选单元格导致读取错误
    cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable->setSelectionMode(QAbstractItemView::SingleSelection);
    cartTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 初始加载 —— 使用不带节流的内部实现，避免构造期间连续调用导致弹窗
    refreshGoodsInternal();
    refreshCartInternal();
    refreshOrdersInternal();

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

    // 使用 Theme 单例统一管理主题（优先使用持久化设置或基于系统）
    Theme::instance().initFromSystem();
}

void UserWindow::onReturnToIdentitySelection() {
    Logger::instance().info("UserWindow::onReturnToIdentitySelection called");
    emit backRequested();
    this->close();
}

// 保存用户信息（密码/地址），手机号不可修改
void UserWindow::onSaveUserInfo() {
    if (!tryThrottle(this)) return;
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
    }
    else {
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
        }
        else {
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
    if (!tryThrottle(this)) return;
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

//
//good

// public wrapper：带节流的刷新商品
void UserWindow::refreshGoods() {
    if (!tryThrottle(this)) return;
    refreshGoodsInternal();
}

// 内部实现（不做节流），原 refreshGoods 的主体逻辑移至此
void UserWindow::refreshGoodsInternal() {
    goodsTable->setRowCount(0);
    if (!client_) return;
    auto goods = client_->CLTgetAllGoods();

    // 读取当前筛选条件（注意：当控件不存在时按默认不过滤）
    std::string nameFilter;
    double minPrice = 0.0;
    double maxPrice = 1e18;
    std::string categoryFilter;
    if (goodsNameFilterEdit) nameFilter = goodsNameFilterEdit->text().toStdString();
    if (priceMinSpin) minPrice = priceMinSpin->value();
    if (priceMaxSpin) maxPrice = priceMaxSpin->value();
    if (categoryFilterEdit) categoryFilter = categoryFilterEdit->text().toStdString();
    if (maxPrice < minPrice) {
        // 交换以保证区间有效
        std::swap(minPrice, maxPrice);
    }

    // 过滤
    std::vector<Good> filtered;
    for (const auto& g : goods) {
        // 名称部分匹配
        if (!containsIC(g.getName(), nameFilter)) continue;
        // 分类部分匹配
        if (!containsIC(g.getCategory(), categoryFilter)) continue;
        // 价格区间
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

void UserWindow::onApplyGoodsFilter() {
    if (!tryThrottle(this)) return;
    // 直接刷新商品表格即可读取筛选条件并应用
    refreshGoodsInternal();
}

void UserWindow::onClearGoodsFilter() {
    if (goodsNameFilterEdit) goodsNameFilterEdit->clear();
    if (priceMinSpin) priceMinSpin->setValue(0.0);
    if (priceMaxSpin) priceMaxSpin->setValue(1e9);
    if (categoryFilterEdit) categoryFilterEdit->clear();
    // 清除时使用内部刷新以避免重复弹窗
    refreshGoodsInternal();
}

//
//cart and promotion
void UserWindow::onAddToCart() {
    if (!tryThrottle(this)) return;
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
        // 如果失败，尝试进一步判断是否为库存不足，并给出友好提示
        Good g;
        if (client_->CLTgetGoodById(id, g) && qty > g.getStock()) {
            QMessageBox::warning(this, "加入购物车", "小店没货啦");
        } else {
            // 更具体的提示并引导查看日志
            QMessageBox::warning(this, "加入购物车", "加入失败（查看 log.txt 获取详细信息）");
        }
    }

    // 刷新显示（内部刷新以避免第二次节流弹窗）
    refreshCartInternal();
}


void UserWindow::refreshCart() {
    if (!tryThrottle(this)) return;
    refreshCartInternal();
}

// 不带节流实现
void UserWindow::refreshCartInternal() {
    cartTable->setRowCount(0);
    if (!client_) {
        Logger::instance().warn("UserWindow::refreshCart: client_ is null");
        return;
    }

    Logger::instance().info(std::string("UserWindow::refreshCart: client connection active? ") + (client_->CLTisConnectionActive() ? "true" : "false"));
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    Logger::instance().info(std::string("UserWindow::refreshCart: got cart_id=") + cart.cart_id + ", items=" + std::to_string(cart.items.size()));

    // 如果购物车没有收货地址，尝试用手机号在账户列表中查找并填充
    if (cart.shipping_address.empty()) {
        try {
            auto users = client_->CLTgetAllAccounts();
            for (const auto& u : users) {
                if (u.getPhone() == phone_) {
                    cart.shipping_address = u.getAddress();
                    Logger::instance().info(std::string("UserWindow::refreshCart: filled shipping_address from account: ") + cart.shipping_address);
                    break;
                }
            }
            // 若找到了地址，保存回服务器（避免下次仍为空）
            if (!cart.shipping_address.empty()) {
                if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
                bool saved = client_->CLTsaveCartForUserWithPolicy(cart, std::string());
                Logger::instance().info(std::string("UserWindow::refreshCart: saved cart with filled address returned ") + (saved ? "true" : "false"));
            }
        } catch (...) {
            Logger::instance().warn("UserWindow::refreshCart: failed to auto-fill shipping_address from accounts");
        }
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

    // 刷新促销列表
    refreshPromotions();
}// 替换现有 refreshCart()，在 cart.shipping_address 为空时尝试从账号填充并保存

void UserWindow::onShowOriginalTotal() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    double original = 0.0;
    for (const auto &it : cart.items) original += it.price * it.quantity;
    PriceWindow dlg("全部原价总额", original, this);
    dlg.exec();
}

void UserWindow::onShowDiscountedTotal() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    double original = 0.0;
    for (const auto &it : cart.items) original += it.price * it.quantity;
    // 优先使用服务器/购物车计算出的 final_amount；若为 0 则视为未应用促销，使用 original
    double discounted = (cart.final_amount > 0.0) ? cart.final_amount : original;
    PriceWindow dlg("全部折后总额", discounted, this);
    dlg.exec();
}

// 从服务器加载促销 JSON 并填充下拉（只读给用户选择）
void UserWindow::refreshPromotions() {
    // 不对 refreshPromotions 本身节流（内部常被调用），以避免被上层操作阻塞
    promoCombo->clear();
    if (!client_) return;
    auto raws = client_->CLTgetAllPromotionsRaw();
    // 第一项占位（不使用促销）
    promoCombo->addItem("-- 使用默认促销(下面首个) --", QString());
    for (const auto &r : raws) {
        std::string name = r.value("name", std::string(""));
        std::string policyStr;
        try {
            if (r.contains("policy")) policyStr = r["policy"].dump();
            else policyStr = r.value("policy_detail", std::string(""));
        } catch (...) {
            policyStr = r.value("policy_detail", std::string(""));
        }
        QString display;
        if (name.empty()) {
            std::string shortPolicy = policyStr.substr(0, (std::min)(policyStr.size(), static_cast<size_t>(60)));
            display = QString::fromStdString(std::string("policy:") + shortPolicy);
        } else {
            display = QString::fromStdString(name);
        }
        promoCombo->addItem(display, QString::fromStdString(policyStr));
    }
    // 强制把下拉恢复到“-- 不使用促销 --”，避免保留上次选择导致默认意外选中某个促销
    promoCombo->setCurrentIndex(0);
}
// 应用所选促销到当前购物车（本地计算 -> 保存）
void UserWindow::onApplyPromotion() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    int idx = promoCombo->currentIndex();
    if (idx <= 0) { QMessageBox::information(this, "应用促销", "请选择有效的促销"); return; }
    QString policyStr = promoCombo->itemData(idx).toString();
    if (policyStr.isEmpty()) { QMessageBox::warning(this, "应用促销", "所选促销数据为空"); return; }

    nlohmann::json policy;
    try { policy = nlohmann::json::parse(policyStr.toStdString()); }
    catch (...) { QMessageBox::warning(this, "应用促销", "促销 policy 解析失败"); return; }

    // 获取当前购物车
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    if (cart.items.empty()) { QMessageBox::information(this, "应用促销", "购物车为空"); return; }

    // 计算原总额与新总额，支持三类策略：discount、full_reduction、tiered
    double original = 0.0;
    int totalQty = 0;
    for (const auto &it : cart.items) { original += it.price * it.quantity; totalQty += it.quantity; }
    double newTotal = original;

    std::string t = policy.value("type", std::string(""));
    if (t == "discount") {
        double d = policy.value("discount", 1.0);
        newTotal = 0.0;
        for (const auto &it : cart.items) newTotal += it.price * it.quantity * d;
    } else if (t == "full_reduction") {
        double threshold = policy.value("threshold", 0.0);
        double reduce = policy.value("reduce", 0.0);
        if (original >= threshold) newTotal = (std::max)(0.0, original - reduce);
        else newTotal = original;
    } else if (t == "tiered") {
        double appliedDiscount = 1.0;
        if (policy.contains("tiers") && policy["tiers"].is_array()) {
            for (const auto &tier : policy["tiers"]) {
                int minq = tier.value("min_qty", 0);
                double disc = tier.value("discount", 1.0);
                if (totalQty >= minq) appliedDiscount = disc;
            }
        }
        newTotal = 0.0;
        for (const auto &it : cart.items) newTotal += it.price * it.quantity * appliedDiscount;
    } else {
        QMessageBox::warning(this, "应用促销", "不支持的促销类型，无法在客户端应用");
        return;
    }

    cart.discount_policy = promoCombo->currentText().toStdString();
    cart.total_amount = original;
    cart.final_amount = newTotal;
    cart.discount_amount = (std::max)(0.0, original - newTotal);

    // 保存到服务器时把 policy JSON 一并发送，避免服务端无法识别仅靠名称的情况
    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    bool ok = client_->CLTsaveCartForUserWithPolicy(cart, policyStr.toStdString());
    if (ok) {
        QMessageBox::information(this, "应用促销", "促销已应用并保存到购物车（含策略数据）");
    } else {
        QMessageBox::information(this, "应用促销", "促销已在界面应用，但保存到服务器失败（请查看日志）");
    }
    refreshCart();
}

// 清除购物车中的促销信息
void UserWindow::onClearPromotion() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    if (cart.items.empty()) { QMessageBox::information(this, "清除促销", "购物车为空"); return; }
    cart.discount_policy.clear();
    cart.final_amount = cart.total_amount;
    cart.discount_amount = 0.0;
    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    // 改为调用带策略的保存接口，传空策略字符串表示不使用任何策略
    bool ok = client_->CLTsaveCartForUserWithPolicy(cart, std::string());
    QMessageBox::information(this, "清除促销", ok ? "已清除并保存" : "清除成功但保存失败");
    refreshCart();
}

// 修改购物车商品数量（已增强日志、重连与重试逻辑）
void UserWindow::onModifyCartItem() {
    if (!tryThrottle(this)) return;
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

    // 安全读取 productId/currentQty，避免 nullptr 解引用
    QTableWidgetItem* idItem = cartTable->item(row, 0);
    QTableWidgetItem* qtyItem = cartTable->item(row, 3);
    if (!idItem || !qtyItem) {
        Logger::instance().warn("UserWindow::onModifyCartItem: table items missing (nullptr)");
        QMessageBox::warning(this, "修改数量", "读取选中行数据失败（请确保整行被选中）");
        return;
    }
    int productId = idItem->text().toInt();
    int currentQty = qtyItem->text().toInt();

    Logger::instance().info(std::string("UserWindow::onModifyCartItem: selected productId=") + std::to_string(productId) + ", currentQty=" + std::to_string(currentQty));
    bool ok;
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
        refreshCartInternal();
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
        // 尝试判断是否为库存不足以提供更友好提示
        Good g;
        if (client_->CLTgetGoodById(productId, g) && newQty > g.getStock()) {
            QMessageBox::warning(this, "修改数量", "小店没货啦");
        } else {
            QMessageBox::warning(this, "修改数量", "更新失败（查看日志）");
        }
    }
    refreshCartInternal();
}

// 删除购物车商品（已增强日志、重连与重试逻辑）
void UserWindow::onRemoveCartItem() {
    if (!tryThrottle(this)) return;
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
        refreshCartInternal();
    } else {
        QMessageBox::warning(this, "删除商品", "删除失败（查看日志）");
    }
}
// --- 新增：作用域禁止窗口交互的 RAII 辅助类（放在文件顶部的 helper 区域） ---
struct ScopedDisableWindow {
    QWidget* w_;
    explicit ScopedDisableWindow(QWidget* w) : w_(w) {
        if (w_) {
            w_->setEnabled(false);
            Logger::instance().info("ScopedDisableWindow: window disabled");
        }
    }
    ~ScopedDisableWindow() {
        if (w_) {
            w_->setEnabled(true);
            Logger::instance().info("ScopedDisableWindow: window re-enabled");
        }
    }
    ScopedDisableWindow(const ScopedDisableWindow&) = delete;
    ScopedDisableWindow& operator=(const ScopedDisableWindow&) = delete;
};

// cart->order
void UserWindow::onCheckout() {
    if (!tryThrottle(this)) return;
    Logger::instance().info("UserWindow::onCheckout called");
    if (!client_) {
        Logger::instance().warn("UserWindow::onCheckout: client_ is null");
        return;
    }

    // 在进入结算逻辑后立即禁止窗口操作，直到本函数退出（成功/失败/取消）。
    // 使用 ScopedDisableWindow 保证在任意 return 路径都能恢复窗口可用性。
    ScopedDisableWindow guard(this);

    // 确保连接
    if (!client_->CLTisConnectionActive()) {
        Logger::instance().warn("UserWindow::onCheckout: client not connected, attempting reconnect");
        client_->CLTreconnect();
    }

    // 1) 读取当前购物车
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    if (cart.items.empty()) {
        QMessageBox::information(this, "结算", "购物车为空，无法结算");
        return;
    }

    // 2) 确保有收货地址：先用账号填充，找不到则要求用户输入
    if (cart.shipping_address.empty()) {
        try {
            auto users = client_->CLTgetAllAccounts();
            for (const auto& u : users) {
                if (u.getPhone() == phone_) {
                    cart.shipping_address = u.getAddress();
                    Logger::instance().info(std::string("UserWindow::onCheckout: filled shipping_address from account: ") + cart.shipping_address);
                    break;
                }
            }
        }
        catch (...) {
            Logger::instance().warn("UserWindow::onCheckout: failed to fetch accounts when filling address");
        }
    }
    if (cart.shipping_address.empty()) {
        bool ok;
        QString addr = QInputDialog::getText(this, "收货地址", "请输入或确认收货地址:", QLineEdit::Normal, QString(), &ok);
        if (!ok) {
            Logger::instance().info("UserWindow::onCheckout: user cancelled address input");
            return;
        }
        addr = addr.trimmed();
        if (addr.isEmpty()) {
            QMessageBox::warning(this, "结算", "收货地址不能为空");
            return;
        }
        cart.shipping_address = addr.toStdString();
        // 尝试保存回服务器（最好保存，以便下一次复用）
        if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
        bool saved = client_->CLTsaveCartForUserWithPolicy(cart, std::string());
        Logger::instance().info(std::string("UserWindow::onCheckout: saved cart with user-entered address -> ") + (saved ? "true" : "false"));
    }

    // 3) 如果用户未主动选择促销，先请求服务器评估促销并短轮询等待更新
    if (cart.discount_policy.empty()) {
        Logger::instance().info("UserWindow::onCheckout: skipping server auto-promote (configured to not request server evaluation)");
    }
    else {
        Logger::instance().info("UserWindow::onCheckout: cart already has discount_policy=\"" + cart.discount_policy + "\" -> using existing policy");
    }

    // 短轮询等待服务器把促销/价格写回购物车（最多等待约 1s）
    TemporaryCart updatedCart;
    const int maxAttempts = 10;
    const double EPS = 1e-4;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
        updatedCart = client_->CLTgetCartForUser(phone_);
        // 判断是否更新：比较 final_amount, discount_amount, discount_policy（字符串）或 items 数量
        bool changed = false;
        if (std::fabs(updatedCart.final_amount - cart.final_amount) > EPS) changed = true;
        if (std::fabs(updatedCart.discount_amount - cart.discount_amount) > EPS) changed = true;
        if (updatedCart.discount_policy != cart.discount_policy) changed = true;
        if (updatedCart.items.size() != cart.items.size()) changed = true;
        if (changed) {
            Logger::instance().info("UserWindow::onCheckout: detected updated cart from server");
            break;
        }
    }
    TemporaryCart useCart = (!updatedCart.items.empty()) ? updatedCart : cart;

    // 4) 使用服务器最终结果（或本地已保存策略）计算应付金额并向用户确认
    double payable = (useCart.final_amount > 0.0) ? useCart.final_amount : useCart.total_amount;
    QString confirmMsg = QString("确认结算当前购物车？应付金额：%1\n收货地址：%2").arg(payable).arg(QString::fromStdString(useCart.shipping_address));
    if (QMessageBox::question(this, "确认结算", confirmMsg) != QMessageBox::Yes) {
        Logger::instance().info("UserWindow::onCheckout: user cancelled checkout");
        return;
    }

    // 5) 生成订单ID（优先用 cart_id）
    // 确保拿到服务器最新的 cart_id（避免使用本地空 id 导致 fallback orderId）
    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    try {
        TemporaryCart fresh = client_->CLTgetCartForUser(phone_);
        if (!fresh.cart_id.empty()) {
            useCart.cart_id = fresh.cart_id;
            Logger::instance().info(std::string("UserWindow::onCheckout: refreshed cart_id from server -> ") + useCart.cart_id);
        }
        else {
            Logger::instance().info("UserWindow::onCheckout: server returned empty cart_id on refresh");
        }
    }
    catch (...) {
        Logger::instance().warn("UserWindow::onCheckout: failed to refresh cart before order id generation");
    }

    QString oid;
    if (!useCart.cart_id.empty()) {
        oid = QString::fromStdString(useCart.cart_id);
        Logger::instance().info(std::string("UserWindow::onCheckout: using cart.cart_id as order id: ") + useCart.cart_id);
    }
    else {
        oid = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz");
        std::mt19937 rng((unsigned)QDateTime::currentMSecsSinceEpoch());
        std::uniform_int_distribution<int> dist(0, 9999);
        oid += QString::number(dist(rng));
        Logger::instance().info(std::string("UserWindow::onCheckout: generated fallback order id: ") + oid.toStdString());
    }

    // 6) 构造 Order 并发送（把 useCart 的 shipping_address / discount_policy / final_amount 等都带上）
    Order order(useCart, oid.toStdString(), 1);
    order.setOrderId(oid.toStdString());
    order.setUserPhone(phone_);
    order.setShippingAddress(useCart.shipping_address);
    order.setDiscountPolicy(useCart.discount_policy);
    order.setTotalAmount(useCart.total_amount);
    order.setDiscountAmount(useCart.discount_amount);
    order.setFinalAmount(useCart.final_amount);

    if (!client_->CLTisConnectionActive()) client_->CLTreconnect();
    Logger::instance().info(std::string("UserWindow::onCheckout: sending order ") + oid.toStdString() + " for user " + phone_);
    bool ok = client_->CLTaddSettledOrder(order);
    Logger::instance().info(std::string("UserWindow::onCheckout CLTaddSettledOrder returned ") + (ok ? "true" : "false"));
    if (!ok) {
        QMessageBox::warning(this, "结算", "生成订单失败（查看日志获取详细信息）");
        return;
    }

    // 7) 清空并保存购物车（与之前行为保持一致）
    TemporaryCart emptyCart = useCart;
    emptyCart.items.clear();
    emptyCart.total_amount = 0.0;
    emptyCart.discount_amount = 0.0;
    emptyCart.final_amount = 0.0;
    emptyCart.discount_policy.clear();
    emptyCart.cart_id.clear();
    emptyCart.is_converted = true;
    bool saved = client_->CLTsaveCartForUserWithPolicy(emptyCart, std::string());
    Logger::instance().info(std::string("UserWindow::onCheckout CLTsaveCartForUser returned ") + (saved ? "true" : "false"));

    QMessageBox::information(this, "结算", "订单已生成");
    // 使用内部刷新以避免再次被节流弹窗
    refreshCartInternal();
    refreshOrdersInternal();
}

//
//order

void UserWindow::refreshOrders() {
    if (!tryThrottle(this)) return;
    refreshOrdersInternal();
}

// 不带节流实现
void UserWindow::refreshOrdersInternal() {
    if (!client_) return;
    orderTable->setRowCount(0);

    // 拉取当前用户的订单（server 支持按 userPhone 查询）
    auto orders = client_->CLTgetAllOrders(phone_);

    // 时间范围
    QDateTime start = orderStartEdit ? orderStartEdit->dateTime() : QDateTime::fromSecsSinceEpoch(0);
    QDateTime end = orderEndEdit ? orderEndEdit->dateTime() : QDateTime::currentDateTime().addYears(10);

    int selStatus = -1;
    if (orderStatusFilter) {
        QVariant v = orderStatusFilter->currentData();
        if (v.isValid()) selStatus = v.toInt();
    }
    // 更健壮的 parseOid：支持 "cYYYYMMDDHHmmsszzz_..."，并避免使用已弃用方法
    auto parseOid = [](const std::string& oidStr) -> QDateTime {
        QString oid = QString::fromStdString(oidStr).trimmed();
        if (!oid.startsWith('c')) return QDateTime();
        if (oid.size() < 18) return QDateTime();
        QString sub = oid.mid(1, 17); // YYYYMMDDHHmmsszzz
        QDateTime dt = QDateTime::fromString(sub, "yyyyMMddHHmmsszzz");
        if (dt.isValid()) { dt = dt.toLocalTime(); return dt; }
        QString datePart = sub.left(14); // YYYYMMDDHHmmss
        QString msPart = sub.mid(14, 3); // zzz
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
        if (selStatus != -1 && o.getStatus() != selStatus) continue;
        QDateTime dt = parseOid(o.getOrderId());
        if (dt.isValid()) {
            if (dt >= start && dt <= end) filtered.push_back(o);
        }
        else {
            filtered.push_back(o);
        }

    }

    orderTable->setRowCount((int)filtered.size());
    for (int i = 0; i < (int)filtered.size(); i++) {
        const Order& o = filtered[i];
        QTableWidgetItem* idItem = new QTableWidgetItem(QString::fromStdString(o.getOrderId()));
        idItem->setData(Qt::UserRole, QString::fromStdString(o.getUserPhone()));
        orderTable->setItem(i, 0, idItem);
        orderTable->setItem(i, 1, new QTableWidgetItem(orderStatusToText(o.getStatus())));
        orderTable->setItem(i, 2, new QTableWidgetItem(QString::number(o.getFinalAmount())));
        QTableWidgetItem* addrItem = new QTableWidgetItem(QString::fromStdString(o.getShippingAddress()));
        addrItem->setData(Qt::UserRole, QString::fromStdString(o.getShippingAddress()));
        orderTable->setItem(i, 3, addrItem);
    }
}

void UserWindow::onViewOrderDetail() {
    if (!tryThrottle(this)) return;
    if (!client_) return;
    auto items = orderTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "查看订单", "请先选择订单行"); return; }
    int row = orderTable->row(items.first());
    QString oid = orderTable->item(row, 0)->text();
    Order detail;
    bool ok = client_->CLTgetOrderDetail(oid.toStdString(), phone_, detail);
    if (!ok) { QMessageBox::warning(this, "查看订单", "获取订单详情失败"); return; }

    // 计算原价总额（如果 server 未返回或为 0，则用项求和作为回退）
    double computedTotal = 0.0;
    for (const auto& it : detail.getItems()) computedTotal += it.getPrice() * it.getQuantity();
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
    for (const auto& it : detail.getItems()) {
        txt += QString::fromStdString(it.getGoodName()) + " (id:" + QString::number(it.getGoodId()) + ") x" + QString::number(it.getQuantity())
               + " 单价:" + QString::number(it.getPrice(), 'f', 2) + " 小计:" + QString::number(it.getSubtotal(), 'f', 2) + "\n";
    }
    QMessageBox::information(this, "订单详情", txt);
}

void UserWindow::onReturnOrder() {
    if (!tryThrottle(this)) return;
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
    if (!tryThrottle(this)) return;
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
    if (!tryThrottle(this)) return;
    if (!client_) return;
    auto items = orderTable->selectedItems();
    if (items.empty()) { QMessageBox::warning(this, "删除订单", "请先选择订单行"); return; }
    int row = orderTable->row(items.first());
    QString oid = orderTable->item(row, 0)->text();
    if (QMessageBox::question(this, "删除订单", "确认删除订单 " + oid + " ? 该操作可能不可恢复") != QMessageBox::Yes) return;
    bool ok = client_->CLTdeleteSettledOrder(oid.toStdString(), phone_);
    QMessageBox::information(this, "删除订单", ok ? "删除成功" : "删除失败（查看日志）");
}



void UserWindow::onApplyOrdersFilter() {
    if (!tryThrottle(this)) return;
    refreshOrdersInternal();
}


void UserWindow::onClearOrdersFilter() {
    if (orderStartEdit) orderStartEdit->setDateTime(QDateTime::fromSecsSinceEpoch(0));
    if (orderEndEdit) orderEndEdit->setDateTime(QDateTime::currentDateTime().addYears(10));
    if (orderStatusFilter) orderStatusFilter->setCurrentIndex(0);
    refreshOrdersInternal();
}


