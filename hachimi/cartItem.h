#pragma once
#include <string>
#include <iostream>
using namespace std;

class CartItem {
public:
    int good_id;
    std::string good_name;   // 对齐订单项
    double price;
    int quantity;
    double subtotal;         // 直接存储小计，便于结算
    // 构造函数、getter/setter 可补充
};