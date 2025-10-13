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
#include "PromotionStrategy.h"
#include "TemporaryCart.h"
#include "logger.h"
#include "admin.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QMutexLocker>
#include <QString>
#include <QByteArray>
#include <nlohmann/json.hpp>

class Client {
private:
    class Impl;
    Impl* pImpl;
    std::recursive_mutex requestMutex;

public:
    Client(const std::string& ip = "127.0.0.1", int port = 8888);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(Client&&) = delete;

    bool CLTconnectToServer();
	bool CLTreconnect();
    void CLTdisconnect();
    bool CLTisConnectionActive() const;

    std::string CLTsendRequest(const std::string& request);

    // ---------------- 商品相关（对应 Server 的商品 API） ----------------
    // 对应 SERgetAllGoods
    std::vector<Good> CLTgetAllGoods();
    // 对应 SERupdateGood
    bool CLTupdateGood(int id, const std::string& name, double price, int stock, const std::string& category);
    // 对应 SERaddGood
    bool CLTaddGood(const std::string& name, double price, int stock, const std::string& category);
    // 对应 SERgetGoodById
    bool CLTgetGoodById(int id, Good& outGood); // 返回是否成功并通过 outGood 输出
    // 对应 SERdeleteGood
    bool CLTdeleteGood(int id);
    // 对应 SERsearchGoodsByCategory
    std::vector<Good> CLTsearchGoodsByCategory(const std::string& category);

    // ---------------- 用户相关（对应 Server 的用户 API） ----------------
    // 对应 SERgetAllAccounts
    std::vector<User> CLTgetAllAccounts();
    // 对应 SERlogin
    bool CLTlogin(const std::string& phone, const std::string& password);
    // 对应 SERupdateAccountPassword
    bool CLTupdateAccountPassword(const std::string& userId, const std::string& oldPassword, const std::string& newPassword);
    // 对应 SERdeleteAccount
    bool CLTdeleteAccount(const std::string& phone, const std::string& password);
    // 对应 SERaddAccount
    bool CLTaddAccount(const std::string& phone, const std::string& password,const std::string& address);
    // 对应 UPDATE_USER (server-side update of phone/password/address)
    bool CLTupdateUser(const std::string& phone, const std::string& password, const std::string& address);

    // ---------------- 购物车相关（对应 Server 的购物车 API） ----------------
    // 对应 SERgetCart
    TemporaryCart CLTgetCartForUser(const std::string& userPhone);
    // 对应 SERsaveCart（包含促销策略 JSON）
    bool CLTsaveCartForUserWithPolicy(const TemporaryCart& cart, const std::string& policyJson);
    // 对应 SERaddToCart
    bool CLTaddToCart(const std::string& userPhone, int productId, const std::string& productName, double price, int quantity);
    // 对应 SERupdateCartItem
    bool CLTupdateCartItem(const std::string& userPhone, int productId, int quantity);
    // 对应 SERremoveFromCart
    bool CLTremoveFromCart(const std::string& userPhone, int productId);
    // 对应 SERupdateCartForPromotions
    bool CLTupdateCartForPromotions(const std::string& userPhone);

    // ---------------- 订单相关（对应 Server 的订单 API） ----------------
    // 对应 SERgetAllOrders
    std::vector<Order> CLTgetAllOrders(const std::string& userPhone = "");
    // 对应 SERgetOrderDetail
    bool CLTgetOrderDetail(const std::string& orderId, const std::string& userPhone, Order& outOrder);
    // 对应 SERupdateOrderStatus
    bool CLTupdateOrderStatus(const std::string& orderId, const std::string& userPhone, int newStatus);
    // 对应 SERaddSettledOrder （保留两种便捷方式）
    bool CLTaddSettledOrder(const Order& order); // 使用 Order 对象
    bool CLTaddSettledOrderRaw(const std::string& orderId, const std::string& productName, int productId,
                               int quantity, const std::string& userPhone, int status, const std::string& discountPolicy = "");
    // 对应 SERreturnSettledOrder
    bool CLTreturnSettledOrder(const std::string& orderId, const std::string& userPhone);
    // 对应 SERrepairSettledOrder
    bool CLTrepairSettledOrder(const std::string& orderId, const std::string& userPhone);
    // 对应 SERdeleteSettledOrder
    bool CLTdeleteSettledOrder(const std::string& orderId, const std::string& userPhone);


    // ---------------- 促销相关（对应 Server 的促销 API） ----------------
    // 改为返回策略对象的智能指针，避免对象切割和抽象类型问题
    std::vector<std::shared_ptr<PromotionStrategy>> CLTgetAllPromotions();
    std::vector<std::shared_ptr<PromotionStrategy>> CLTgetPromotionsByProductId(int productId);
    // 获取原始促销列表（用于 UI 显示 id/name/policy JSON）
    std::vector<nlohmann::json> CLTgetAllPromotionsRaw();
    // 管理员：增删改促销（客户端以管理员身份调用）
    bool CLTaddPromotion(const nlohmann::json& promotion); // promotion JSON { name, policy, type?, conditions? }
    bool CLTupdatePromotion(const std::string& name, const nlohmann::json& promotion);
    bool CLTdeletePromotion(const std::string& name);

};