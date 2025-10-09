#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "good.h"
#include "user.h"
#include "cartItem.h"
#include "order.h"
#include "orderItem.h"
#include "promotion.h"
#include "TemporaryCart.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QMutexLocker>
#include <QString>
#include <QByteArray>

class TCPClient {
private:
    class Impl;
    Impl* pImpl;
    std::recursive_mutex requestMutex;

public:
    TCPClient(const std::string& ip = "127.0.0.1", int port = 8888);
    ~TCPClient();

    TCPClient(const TCPClient&) = delete;
    TCPClient& operator=(const TCPClient&) = delete;
    TCPClient(TCPClient&&) = delete;
    TCPClient& operator=(TCPClient&&) = delete;

    bool connectToServer();
    void disconnect();
    bool isConnectionActive() const;

    std::string sendRequest(const std::string& request);

    // 商品相关
    std::vector<Good> getAllGoods();
    bool updateGood(int id, const std::string& name, double price, int stock, const std::string& category);

    // 用户相关
    std::vector<User> getAllUsers();
    bool addUser(const std::string& phone, const std::string& password);

    // 购物车相关（推荐：一次性保存/加载整个购物车）
    TemporaryCart getCartForUser(const std::string& userPhone);
    bool saveCartForUser(const TemporaryCart& cart);
    bool updateCartForPromotions(const std::string& userPhone);

    // 订单相关
    bool addSettledOrder(const Order& order);
    std::vector<Order> getSettledOrders(const std::string& userPhone = "");
    std::vector<Order> getSettledOrdersByStatus(int status);
    std::vector<Order> getCompletedOrdersForUser(const std::string& userPhone);
    bool deleteSettledOrder(const std::string& orderId, const std::string& userPhone);
    bool returnSettledOrder(const std::string& orderId, const std::string& userPhone);
    bool repairSettledOrder(const std::string& orderId, const std::string& userPhone);

    // 促销相关
    std::vector<PromotionStrategy> getPromotions();
    std::vector<PromotionStrategy> getProductPromotions(int productId);
};