#pragma once
#include <string>
#include <iostream>
#include "good.h"
//using namespace std;

class CartItem {
public:
    int good_id;
    std::string good_name;   // 对齐订单项
    double price;
    int quantity;
    double subtotal;         // 直接存储小计，便于结算

    // 默认构造函数
    CartItem()
        : good_id(0), good_name(""), price(0.0), quantity(0), subtotal(0.0) {}

    // 通过 Good 构造
    CartItem(const Good& good, int qty)
        : good_id(good.getId()), good_name(good.getName()), price(good.getPrice()), quantity(qty) {
        subtotal = price * quantity;
    }
};