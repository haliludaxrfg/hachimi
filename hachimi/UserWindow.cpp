#include "UserWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QDateTime>

UserWindow::UserWindow(const std::string& phone, Client* client, QWidget* parent)
    : QWidget(parent), phone_(phone), client_(client)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    tabWidget = new QTabWidget(this);

    // 1. 用户信息管理
    userTab = new QWidget;
    QVBoxLayout* userLayout = new QVBoxLayout(userTab);
    userLayout->addWidget(new QLabel("这里可以显示和修改用户个人信息（如手机号、密码、地址等）"));
    // 可添加 QLineEdit、保存按钮等控件

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
    checkoutBtn = new QPushButton("结算（生成订单）");
    cartBtnLayout->addWidget(refreshCartBtn);
    cartBtnLayout->addWidget(checkoutBtn);
    goodsLayout->addLayout(cartBtnLayout);

    // 3. 订单查看
    orderTab = new QWidget;
    QVBoxLayout* orderLayout = new QVBoxLayout(orderTab);
    orderLayout->addWidget(new QLabel("已完成订单列表"));
    // 可用 QTableWidget 展示订单信息

    tabWidget->addTab(userTab, "个人信息");
    tabWidget->addTab(goodsTab, "商品与购物车");
    tabWidget->addTab(orderTab, "我的订单");

    mainLayout->addWidget(tabWidget);

    // 返回按钮
    backBtn = new QPushButton("返回上一级", this);
    connect(backBtn, &QPushButton::clicked, this, &UserWindow::close);
    mainLayout->addWidget(backBtn);

    // 连接槽
    connect(refreshGoodsBtn, &QPushButton::clicked, this, &UserWindow::refreshGoods);
    connect(addToCartBtn, &QPushButton::clicked, this, &UserWindow::onAddToCart);
    connect(refreshCartBtn, &QPushButton::clicked, this, &UserWindow::refreshCart);
    connect(checkoutBtn, &QPushButton::clicked, this, &UserWindow::onCheckout);

    // 初始加载
    refreshGoods();
    refreshCart();
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

    bool res = client_->CLTaddToCart(phone_, id, name.toStdString(), price, qty);
    QMessageBox::information(this, "加入购物车", res ? "已加入购物车" : "加入失败");
    refreshCart();
}

void UserWindow::refreshCart() {
    cartTable->setRowCount(0);
    if (!client_) return;
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    cartTable->setRowCount((int)cart.items.size());
    for (int i = 0; i < (int)cart.items.size(); ++i) {
        const CartItem& it = cart.items[i];
        cartTable->setItem(i, 0, new QTableWidgetItem(QString::number(it.good_id)));
        cartTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(it.good_name)));
        cartTable->setItem(i, 2, new QTableWidgetItem(QString::number(it.price)));
        cartTable->setItem(i, 3, new QTableWidgetItem(QString::number(it.quantity)));
        cartTable->setItem(i, 4, new QTableWidgetItem(QString::number(it.subtotal)));
    }
}

void UserWindow::onCheckout() {
    if (!client_) return;
    TemporaryCart cart = client_->CLTgetCartForUser(phone_);
    if (cart.items.empty()) {
        QMessageBox::information(this, "结算", "购物车为空");
        return;
    }
    // 使用临时购物车的 cart_id 作为订单 ID
    QString orderId = QString::fromStdString(cart.cart_id);

    // 使用从购物车生成订单的构造函数，将 cart_id 传入
    Order order(cart, orderId.toStdString(), 1);

    // 将整个 Order 提交到后端
    bool ok = client_->CLTaddSettledOrder(order);
    if (ok) {
        QMessageBox::information(this, "结算", "结算成功（已提交订单）");
        // 清空购物车：用后台接口删除购物车内商品
        for (const auto& it : cart.items) {
            client_->CLTremoveFromCart(phone_, it.good_id);
        }
        refreshCart();
    } else {
        QMessageBox::warning(this, "结算", "结算失败，请重试");
    }
}