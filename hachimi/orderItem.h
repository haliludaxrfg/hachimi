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
};
