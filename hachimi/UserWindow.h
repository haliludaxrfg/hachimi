#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>

class UserWindow : public QWidget {
    Q_OBJECT
public:
    UserWindow(QWidget* parent = nullptr);

signals:
    void backRequested(); // 返回上一级信号

private:
    QTabWidget* tabWidget;
    QWidget* userTab;   // 个人信息管理
    QWidget* goodsTab;  // 商品浏览与购物车
    QWidget* orderTab;  // 订单查看
    QPushButton* backBtn;
    // 你可以在这里添加更多控件成员，如商品表、购物车表、订单表等
};