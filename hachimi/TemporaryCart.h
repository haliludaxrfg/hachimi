#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include "CartItem.h"
#include "good.h"


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

    // 添加商品到购物车
    void addGood(const CartItem& item);

    // 删除商品
    bool removeGood(int good_id);

    // 查找商品
    CartItem* findGood(int good_id);

    // 修改商品
    bool updateGood(int good_id, const CartItem& item);

    // 显示所有商品
    void showGoods() const;

    // 计算总价
    double totalPrice() const;

    // 计算促销后总价
    double calculatePromotionTotal() const;

    // 清空购物车
    void clearGoods();
};