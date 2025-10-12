#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include "Client.h"
#include "TemporaryCart.h"

class AdminWindow : public QWidget {
    Q_OBJECT
public:
    // 改为接收 Client* 以通过网络/API 操作数据
    AdminWindow(Client* client, QWidget* parent = nullptr);

signals:
    void backRequested(); // 返回信号

private slots:
    void refreshUsers();
	void onAddUser();
	void onEditUser();
    void onDeleteUser();
    void onSearchUser(); // 新增：依据手机号查询用户
    void refreshGoods();
    void onAddGood();
    void onEditGood();
    void onDeleteGood();
    void refreshOrders();

    // 新增：返回身份选择界面槽
    void onReturnToIdentitySelection();

    // 购物车相关槽（已移除 Add/Save）
    void onLoadCartForUser();
    void onEditCartItem();
    void onRemoveCartItem();

private:
    QTabWidget* tabWidget;
    QWidget* userTab;
    QWidget* goodTab;
    QWidget* orderTab;
    QPushButton* backBtn;

    // 新增 UI 成员
    Client* client_;
    QTableWidget* usersTable;
    QPushButton* refreshUsersBtn;
    QPushButton* addUserBtn;    // 新增：添加用户按钮
    QPushButton* editUserBtn;   // 新增：修改用户按钮
    QPushButton* deleteUserBtn;
    QPushButton* searchUserBtn; // 新增：查询用户按钮

    // 新增：返回身份选择按钮
    QPushButton* returnIdentityBtn;

    QTableWidget* goodsTable;
    QPushButton* refreshGoodsBtn;
    QPushButton* addGoodBtn;
    QPushButton* editGoodBtn;
    QPushButton* deleteGoodBtn;

    QTableWidget* ordersTable;
    QPushButton* refreshOrdersBtn;

    // ------- 购物车管理 UI -------
    QWidget* cartTab;
    QLineEdit* cartPhoneEdit;
    QPushButton* loadCartBtn;
    QTableWidget* cartTable; // good_id, name, price, qty, subtotal
    QPushButton* editCartItemBtn;
    QPushButton* removeCartItemBtn;

    // 当前加载的购物车缓存
    TemporaryCart currentCart_;
};