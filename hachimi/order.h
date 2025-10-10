#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <vector>
#include "TemporaryCart.h"
#include  "orderItem.h"
//using namespace std;
class Order {
	friend class OrderItem;
private:
    std::string order_id;          // 主键
    std::string user_phone;        // 用户ID
    std::string shipping_address;  // 收货地址
    int status;                    // 订单状态
    std::string discount_policy;   // 优惠策略
    double total_amount;           // 总金额
    double discount_amount;        // 优惠金额
    double final_amount;           // 实付金额
    std::vector<OrderItem> items;       // 订单项列表
public:
	// 构造函数 从购物车生成订单
    Order(const TemporaryCart& cart, const std::string& new_order_id, int order_status = 1);
	// 显示订单详情
	void showOrder() const;
	// getXX
    const std::string& getOrderId() const { return order_id; }
    const std::string& getUserPhone() const { return user_phone; }
    const std::string& getShippingAddress() const { return shipping_address; }
    int getStatus() const { return status; }
    const std::string& getDiscountPolicy() const { return discount_policy; }
    double getTotalAmount() const { return total_amount; }
    double getDiscountAmount() const { return discount_amount; }
    double getFinalAmount() const { return final_amount; }
    const std::vector<OrderItem>& getItems() const { return items; }
    //setXX
	void setItems(const std::vector<OrderItem>& newItems) { items = newItems; }
	void setStatus(int newStatus) { status = newStatus; }
	void setShippingAddress(const std::string& newAddress) { shipping_address = newAddress; }
	void setDiscountPolicy(const std::string& newPolicy) { discount_policy = newPolicy; }


};