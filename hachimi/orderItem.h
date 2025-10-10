#pragma once
#include <string>
#include <iostream>
#include  "order.h"
//using namespace std;
class OrderItem {
	friend class Order;
private:
    std::string order_id;          // 外键，关联订单
    int good_id;                   // 商品ID
    std::string good_name;         // 商品名称（快照）
    double price;                  // 商品价格（快照）
    int quantity;                  // 购买数量
    double subtotal;               // 小计金额
public:
    // getXX
    const std::string& getOrderId() const { return order_id; }
    int getGoodId() const { return good_id; }
    const std::string& getGoodName() const { return good_name; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }
    double getSubtotal() const { return subtotal; }

    // setXX
    void setOrderId(const std::string& id) { order_id = id; }
    void setGoodId(int id) { good_id = id; }
    void setGoodName(const std::string& name) { good_name = name; }
    void setPrice(double p) { price = p; }
    void setQuantity(int q) { quantity = q; }
    void setSubtotal(double s) { subtotal = s; }
};
