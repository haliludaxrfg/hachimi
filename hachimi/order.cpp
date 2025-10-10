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

Order::Order(const TemporaryCart& cart, const std::string& new_order_id, int order_status)
	: order_id(new_order_id), user_phone(cart.user_phone), shipping_address(cart.shipping_address),
	  status(order_status), discount_policy(cart.discount_policy),
	  total_amount(cart.total_amount), discount_amount(cart.discount_amount),
	  final_amount(cart.final_amount), items() {
	for (const auto& ci : cart.items) {
		OrderItem oi;
		oi.order_id = order_id;
		oi.good_id = ci.good_id;
		oi.good_name = ci.good_name;
		oi.price = ci.price;
		oi.quantity = ci.quantity;
		oi.subtotal = ci.subtotal;
		items.push_back(oi);
	}
}