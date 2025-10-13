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
#include "PromotionStrategy.h"
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

    bool DTBconnect();
    void DTBdisconnect();
    bool DTBexecuteQuery(const std::string& query);
    MYSQL_RES* DTBexecuteSelect(const std::string& query);

public:
    DatabaseManager(const std::string& host, const std::string& user,
        const std::string& password, const std::string& database,
        unsigned int port = 3306);
    ~DatabaseManager();

    // 连接管理
    bool DTBinitialize();
    bool DTBisConnected() const;

    // 用户管理
    bool DTBaddUser(const User& u); // 新增用户
    bool DTBsaveUser(const User& u);
    bool DTBupdateUser(const User& u);
    bool DTBloadUser(const std::string& phone, User& u);
    bool DTBdeleteUser(const std::string& phone);
    std::vector<User> DTBloadAllUsers();

    // 商品管理
    bool DTBsaveGood(const Good& g);
    bool DTBupdateGood(const Good& g);
    bool DTBloadGood(int id, Good& g);
    bool DTBdeleteGood(int id);
    std::vector<Good> DTBloadAllGoods();
    std::vector<Good> DTBloadGoodsByCategory(const std::string& category);
    bool DTBupdateGoodStock(int good_id, int new_stock);

    // 订单管理
    bool DTBsaveOrder(const Order& o);
    bool DTBupdateOrder(const Order& o);
    bool DTBupdateOrderStatus(const std::string& order_id, int status);
    bool DTBloadOrder(const std::string& order_id, Order& o);
    bool DTBdeleteOrder(const std::string& order_id);
    std::vector<Order> DTBloadOrdersByUser(const std::string& user_phone);
    std::vector<Order> DTBloadOrdersByStatus(int status);
    std::vector<Order> DTBloadRecentOrders(int limit = 50);

    // 订单项管理
    bool DTBsaveOrderItem(const OrderItem& item);
    bool DTBupdateOrderItem(const OrderItem& item);
    std::vector<OrderItem> DTBloadOrderItems(const std::string& order_id);
    bool DTBdeleteOrderItems(const std::string& order_id);

    // 临时购物车管理
 
    bool DTBloadTemporaryCart(const std::string& cart_id, TemporaryCart& cart);
    // 按用户手机号查找最新的临时购物车（返回 true 并填充 outCart 表示找到）
    bool DTBloadTemporaryCartByUserPhone(const std::string & userPhone, TemporaryCart & outCart);
    bool DTBdeleteTemporaryCart(const std::string& cart_id);
    bool DTBsaveTemporaryCart(const TemporaryCart& cart);
    bool DTBupdateTemporaryCart(const TemporaryCart& cart);
    std::vector<TemporaryCart> DTBloadExpiredCarts();
    bool DTBcleanupExpiredCarts();

    // 购物车项管理
    bool DTBsaveCartItem(const CartItem& item, const std::string& cart_id);
    bool DTBupdateCartItem(const CartItem& item, const std::string& cart_id);
    bool DTBdeleteCartItem(int good_id, const std::string& cart_id);
    bool DTBdeleteAllCartItems(const std::string& cart_id);
    std::vector<CartItem> DTBloadCartItems(const std::string& cart_id);

    // 促销策略
    bool DTBsavePromotionStrategy(const std::string& name, const std::string& type,
        const std::string& config, const std::string& conditions = "");
    bool DTBupdatePromotionStrategy(const std::string& name, bool is_active);
    std::map<std::string, std::string> DTBloadPromotionStrategy(const std::string& name);
    std::vector<std::map<std::string, std::string>> DTBloadAllPromotionStrategies(bool active_only = true);
    bool DTBupdatePromotionStrategyDetail(const std::string& name, const std::string& policy_detail);
    bool DTBdeletePromotionStrategy(const std::string& name);
};