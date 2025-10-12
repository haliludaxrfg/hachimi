#pragma once
#include "cartItem.h"
#include "good.h"
#include "order.h"
#include "orderItem.h"
#include "TemporaryCart.h"
#include "user.h"
#include "userManager.h"
#include "databaseManager.h"
#include "PromotionStrategy.h"
#include "logger.h"

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QObject>
#include <QDebug>

// 支持异步多客户端的Server声明
class Server : public QObject {
    Q_OBJECT
    DatabaseManager* dbManager;
    int port;
    QTcpServer* server;

    // 生成符合数据库要求的购物车 ID（基于时间 + 随机数，长度 <= 64）
    std::string generateCartId(const std::string& userPhone);

public:
    Server(int port);
    ~Server();

    // 启动服务器
    void SERstart();

    // 停止服务器
    void SERstop();

    // 处理客户端请求
    std::string SERprocessRequest(const std::string& request);

    // 业务处理接口
    std::string SERgetAllGoods();
    std::string SERupdateGood(int GoodId, std::string Goodname, double Goodprice, int Goodstock, std::string Goodcategory);
    std::string SERaddGood(const std::string& name, double price, int stock, const std::string& category);
    std::string SERgetGoodById(int id);
    std::string SERdeleteGood(int id);
    std::string SERsearchGoodsByCategory(const std::string& category);
    //user
    std::string SERgetAllAccounts();
    std::string SERlogin(const std::string& phone, const std::string& password);
    std::string SERupdateAccountPassword(const std::string& userId, const std::string& oldPassword, const std::string& newPassword);
    std::string SERdeleteAccount(const std::string& phone, const std::string& password);
    std::string SERaddAccount(const std::string& phone, const std::string& password,const std::string& address);
    std::string SERupdateUser(const std::string& phone, const std::string& password, const std::string& address);
	//order
    std::string SERgetAllOrders(const std::string& userPhone);
    std::string SERgetOrderDetail(const std::string& orderId, const std::string& userPhone);
    std::string SERupdateOrderStatus(const std::string& orderId, const std::string& userPhone, int newStatus);
    std::string SERaddSettledOrder(const std::string& orderId, const std::string& productName, int productId,
                                int quantity, const std::string& userPhone, int status, const std::string& discountPolicy = "");
    std::string SERreturnSettledOrder(const std::string& orderId, const std::string& userPhone);
    std::string SERrepairSettledOrder(const std::string& orderId, const std::string& userPhone);
    std::string SERdeleteSettledOrder(const std::string& orderId, const std::string& userPhone);
	//cart
    std::string SERgetCart(const std::string& userPhone);
    std::string SERsaveCart(const std::string& userPhone, const std::string& cartData);
    std::string SERaddToCart(const std::string& userPhone, int productId, const std::string& productName, double price, int quantity);
    std::string SERupdateCartItem(const std::string& userPhone, int productId, int quantity);
    std::string SERremoveFromCart(const std::string& userPhone, int productId);
    static void recalcCartTotals(TemporaryCart& cart);
	// promotion
    std::string SERgetAllPromotions();
    std::string SERgetPromotionsByProductId(int productId);
    std::string SERupdateCartForPromotions(const std::string& userPhone);
};
