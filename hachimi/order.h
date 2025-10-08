#pragma once
#include <string>
#include <memory>
#include <iostream>
using namespace std;
class Order {
private:
    std::string order_id;          // 主键
    std::string user_phone;        // 用户ID
    std::string shipping_address;  // 收货地址
    int status;                    // 订单状态
    std::string discount_policy;   // 优惠策略
    double total_amount;           // 总金额
    double discount_amount;        // 优惠金额
    double final_amount;           // 实付金额

};