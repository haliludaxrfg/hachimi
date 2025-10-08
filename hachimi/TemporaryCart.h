#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include "CartItem.h"
using namespace std;

class TemporaryCart {
public:
    std::string cart_id;
    std::string user_phone;
    std::string shipping_address;   // 对齐订单
    std::string discount_policy;    // 对齐订单
    double total_amount;            // 对齐订单
    double discount_amount;         // 对齐订单
    double final_amount;            // 对齐订单
    std::vector<CartItem> items;
    bool is_converted;
};