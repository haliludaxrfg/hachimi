#include "AdminWindow.h"
#include <QHBoxLayout>
#include <QLabel>

AdminWindow::AdminWindow(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    tabWidget = new QTabWidget(this);

    // 用户管理页
    userTab = new QWidget;
    userTab->setLayout(new QVBoxLayout);
    userTab->layout()->addWidget(new QLabel("这里是用户管理功能区"));

    // 商品管理页
    goodTab = new QWidget;
    goodTab->setLayout(new QVBoxLayout);
    goodTab->layout()->addWidget(new QLabel("这里是商品管理功能区"));

    // 订单管理页
    orderTab = new QWidget;
    orderTab->setLayout(new QVBoxLayout);
    orderTab->layout()->addWidget(new QLabel("这里是订单管理功能区"));

    tabWidget->addTab(userTab, "用户管理");
    tabWidget->addTab(goodTab, "商品管理");
    tabWidget->addTab(orderTab, "订单管理");

    mainLayout->addWidget(tabWidget);

    // 返回按钮
    backBtn = new QPushButton("返回上一级", this);
    connect(backBtn, &QPushButton::clicked, this, &AdminWindow::backRequested);
    mainLayout->addWidget(backBtn);
}