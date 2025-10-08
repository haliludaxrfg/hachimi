#pragma once
#include <mysql.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "user.h"
#include "good.h"
#include "order.h"
#include "orderItem.h"
#include "TemporaryCart.h"
#include "cartItem.h"
#include "promotion.h"
#include <nlohmann/json.hpp>
using nlohmann::json;
class DatabaseManager {
private:
    MYSQL* connection_;
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    unsigned int port_;

    bool connect();
    void disconnect();
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelect(const std::string& query);

public:
    DatabaseManager(const std::string& host, const std::string& user,
        const std::string& password, const std::string& database,
        unsigned int port = 3306);
    ~DatabaseManager();

    // 连接管理
    bool initialize();
    bool isConnected() const;

    // 促销策略
    bool savePromotionStrategy(const std::string& name, const std::string& type,
        const std::string& config, const std::string& conditions = "");
    bool updatePromotionStrategy(const std::string& name, bool is_active);
    std::map<std::string, std::string> loadPromotionStrategy(const std::string& name);
    std::vector<std::map<std::string, std::string>> loadAllPromotionStrategies(bool active_only = true);

    // 用户管理
    bool saveUser(const User& u);
    bool updateUser(const User& u);
    bool loadUser(const std::string& phone, User& u);
    bool deleteUser(const std::string& phone);
    std::vector<User> loadAllUsers();

    // 商品管理
    bool saveGood(const Good& g);
    bool updateGood(const Good& g);
    bool loadGood(int id, Good& g);
    bool deleteGood(int id);
    std::vector<Good> loadAllGoods();
    std::vector<Good> loadGoodsByCategory(const std::string& category);
    bool updateGoodStock(int good_id, int new_stock);

    // 订单管理
    bool saveOrder(const Order& o);
    bool updateOrderStatus(const std::string& order_id, int status);
    bool loadOrder(const std::string& order_id, Order& o);
    bool deleteOrder(const std::string& order_id);
    std::vector<Order> loadOrdersByUser(const std::string& user_phone);
    std::vector<Order> loadOrdersByStatus(int status);
    std::vector<Order> loadRecentOrders(int limit = 50);

    // 订单项管理
    bool saveOrderItem(const OrderItem& item);
    std::vector<OrderItem> loadOrderItems(const std::string& order_id);
    bool deleteOrderItems(const std::string& order_id);

    // 临时购物车管理
    bool saveTemporaryCart(const TemporaryCart& cart);
    bool updateTemporaryCart(const TemporaryCart& cart);
    bool loadTemporaryCart(const std::string& cart_id, TemporaryCart& cart);
    bool deleteTemporaryCart(const std::string& cart_id);
    std::vector<TemporaryCart> loadExpiredCarts();
    bool cleanupExpiredCarts();

    // 购物车项管理
    bool saveCartItem(const CartItem& item, const std::string& cart_id);
    bool updateCartItem(const CartItem& item, const std::string& cart_id);
    bool deleteCartItem(int good_id, const std::string& cart_id);
    bool deleteAllCartItems(const std::string& cart_id);
    std::vector<CartItem> loadCartItems(const std::string& cart_id);

    // 统计和报表
    int getTotalUserCount();
    int getTotalOrderCount();
    double getTotalRevenue();
    std::map<std::string, double> getSalesByCategory();
    std::vector<std::map<std::string, std::string>> getTopSellingGoods(int limit = 10);
};