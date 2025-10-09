#include "order.h"
#include <iostream>
#include "orderItem.h"
void Order::showOrder() const {
	std::cout << "订单ID: " << order_id << std::endl;
	std::cout << "用户电话: " << user_phone << std::endl;
	std::cout << "收货地址: " << shipping_address << std::endl;
	std::cout << "订单状态: " << status << std::endl;
	std::cout << "优惠策略: " << discount_policy << std::endl;
	std::cout << "总金额: " << total_amount << std::endl;
	std::cout << "优惠金额: " << discount_amount << std::endl;
	std::cout << "实付金额: " << final_amount << std::endl;
	std::cout << "订单项列表:" << std::endl;
	for (const auto& item : items) {
		std::cout << "  商品ID: " << item.good_id
				  << ", 商品名称: " << item.good_name
				  << ", 价格: " << item.price
				  << ", 数量: " << item.quantity
				  << ", 小计: " << item.subtotal
				  << std::endl;
	}
}