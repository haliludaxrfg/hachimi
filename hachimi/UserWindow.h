#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QString>
#include "Client.h"

class UserWindow : public QWidget {
    Q_OBJECT
public:
    // 支持按手机号与 Client 操作
    UserWindow(const std::string& phone = "", Client* client = nullptr, QWidget* parent = nullptr);

signals:
    void backRequested(); // 返回上一级信号

private slots:
    void refreshGoods();
    void onAddToCart();
    void refreshCart();
    void onCheckout();

private:
    QTabWidget* tabWidget;
    QWidget* userTab;   // 个人信息管理
    QWidget* goodsTab;  // 商品浏览与购物车
    QWidget* orderTab;  // 订单查看
    QPushButton* backBtn;

    // 内部状态
    std::string phone_;
    Client* client_;
    QTableWidget* goodsTable;
    QPushButton* refreshGoodsBtn;
    QPushButton* addToCartBtn;

    QTableWidget* cartTable;
    QPushButton* refreshCartBtn;
    QPushButton* checkoutBtn;
};