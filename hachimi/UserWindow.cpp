#include "UserWindow.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QMessageBox>

UserWindow::UserWindow(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QTabWidget* tabWidget = new QTabWidget(this);

    // 1. 用户信息管理
    QWidget* userTab = new QWidget;
    QVBoxLayout* userLayout = new QVBoxLayout(userTab);
    userLayout->addWidget(new QLabel("这里可以显示和修改用户个人信息（如手机号、密码、地址等）"));
    // 可添加 QLineEdit、保存按钮等控件

    // 2. 商品浏览与购物车
    QWidget* goodsTab = new QWidget;
    QVBoxLayout* goodsLayout = new QVBoxLayout(goodsTab);
    goodsLayout->addWidget(new QLabel("商品列表（可选中商品并加入购物车）"));
    // 可用 QTableWidget 展示商品，添加“加入购物车”按钮
    goodsLayout->addWidget(new QLabel("购物车（可结算）"));
    // 可用 QTableWidget 展示购物车内容，添加“结算”按钮

    // 3. 订单查看
    QWidget* orderTab = new QWidget;
    QVBoxLayout* orderLayout = new QVBoxLayout(orderTab);
    orderLayout->addWidget(new QLabel("已完成订单列表"));
    // 可用 QTableWidget 展示订单信息

    tabWidget->addTab(userTab, "个人信息");
    tabWidget->addTab(goodsTab, "商品与购物车");
    tabWidget->addTab(orderTab, "我的订单");

    mainLayout->addWidget(tabWidget);

    // 返回按钮
    QPushButton* backBtn = new QPushButton("返回上一级", this);
    connect(backBtn, &QPushButton::clicked, this, &UserWindow::close);
    mainLayout->addWidget(backBtn);
}