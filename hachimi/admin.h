#pragma once
#include <vector>
#include "user.h"
#include "good.h"
#include "order.h"
#include "databaseManager.h"

class Admin {
public:
    static const std::string password;
    Admin(DatabaseManager* db);

    // 用户管理
    bool addUser(const User& user);
	bool updateUser(const User& user);
    bool removeUser(const std::string& phone);
    User* findUser(const std::string& phone);
    std::vector<User>& getAllUsers();

    // 商品管理
    bool addGood(const Good& good);
	bool updateGood(const Good& good);
    bool removeGood(int good_id);
    Good* findGood(int good_id);
    std::vector<Good>& getAllGoods();

    // 订单管理
    bool addOrder(const Order& order);
	bool updateOrder(const Order& order);
    bool removeOrder(const std::string& order_id);
    Order* findOrder(const std::string& order_id);
    std::vector<Order>& getAllOrders();

    // 数据同步
    void loadAllData();
    void saveAllData();

private:
    DatabaseManager* db;
    std::vector<User> users;
    std::vector<Good> goods;
    std::vector<Order> orders;
};
