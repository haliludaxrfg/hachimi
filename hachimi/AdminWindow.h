#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QLabel>
#include "Client.h"

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
    void refreshGoods();
    void onAddGood();
    void onEditGood();
    void onDeleteGood();
    void refreshOrders();

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

    QTableWidget* goodsTable;
    QPushButton* refreshGoodsBtn;
    QPushButton* addGoodBtn;
    QPushButton* editGoodBtn;
    QPushButton* deleteGoodBtn;

    QTableWidget* ordersTable;
    QPushButton* refreshOrdersBtn;
};