#pragma once
#include "cartItem.h"
#include "good.h"
#include "order.h"
#include "orderItem.h"
#include "TemporaryCart.h"
#include "user.h"
#include "userManager.h"
#include "databaseManager.h"
#include "promotion.h"
#include "logger.h"
#include <string>
#include <vector>
#include <iostream>
// 简化版C++ TCP服务器头文件，专门用于MySQL业务管理
class Server {
	DatabaseManager* dbManager;
    int port;
public:
    Server(int port);
    ~Server();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 处理客户端请求
    std::string processRequest(const std::string& request);

    // 业务处理接口（与Java命令风格一致）
    std::string getAllGoods();
	std::string updateGoods(int GoodId, std::string Goodname,double Goodprice,int Goodstock,std::string Goodcategory);

    std::string getAllAccounts();
    std::string login(const std::string& phone, const std::string& password);
    std::string updateAccountPassword(const std::string& userId, const std::string& oldPassword, const std::string& newPassword);
    std::string deleteAccount(const std::string& phone, const std::string& password);
    std::string addAccount(const std::string& phone, const std::string& password);

    std::string getAllOrders(const std::string& userPhone);
    std::string getOrderDetail(const std::string& orderId, const std::string& userPhone);
    std::string updateOrderStatus(const std::string& orderId, const std::string& userPhone, int newStatus);
    std::string addSettledOrder(const std::string& orderId, const std::string& productName, int productId,
                                int quantity, const std::string& userPhone, int status, const std::string& discountPolicy = "");
	std::string returnSettledOrder(const std::string& orderId, const std::string& userPhone);// 退货
	std::string repairSettledOrder(const std::string& orderId, const std::string& userPhone);// 换货
	std::string deleteSettledOrder(const std::string& orderId, const std::string& userPhone);// 删除订单

    std::string getCart(const std::string& userPhone);
    std::string saveCart(const std::string& userPhone, const std::string& cartData);
    std::string addToCart(const std::string& userPhone, int productId, const std::string& productName, double price, int quantity);
    std::string updateCartItem(const std::string& userPhone, int productId, int quantity);
    std::string removeFromCart(const std::string& userPhone, int productId);
    std::string updateGood(int goodId, const std::string& name, double price, int stock);

    std::string getAllPromotions();
    std::string getPromotionsByProductId(int productId);
    std::string updateCartForPromotions(const std::string& userPhone);
};
