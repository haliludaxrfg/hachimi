#include "TemporaryCart.h"

void TemporaryCart::addGood(const CartItem& item) {
	auto it = std::find_if(items.begin(), items.end(), [&](const CartItem& ci) { return ci.good_id == item.good_id; });
	if (it != items.end()) {
		it->quantity += item.quantity;
		it->subtotal = it->price * it->quantity;
	} else {
		items.push_back(item);
	}
}

bool TemporaryCart::removeGood(int good_id) {
	auto it = std::find_if(items.begin(), items.end(), [&](const CartItem& ci) { return ci.good_id == good_id; });
	if (it != items.end()) {
		items.erase(it);
		return true;
	}
	return false;
}

CartItem* TemporaryCart::findGood(int good_id) {
	auto it = std::find_if(items.begin(), items.end(), [&](const CartItem& ci) { return ci.good_id == good_id; });
	if (it != items.end()) {
		return &(*it);
	}
	return nullptr;
}

bool TemporaryCart::updateGood(int good_id, const CartItem& item) {
	auto it = std::find_if(items.begin(), items.end(), [&](const CartItem& ci) { return ci.good_id == good_id; });
	if (it != items.end()) {
		it->quantity = item.quantity;
		it->subtotal = it->price * it->quantity;
		return true;
	}
	return false;
}

void TemporaryCart::showGoods() const {
	std::cout << "购物车ID: " << cart_id << std::endl;
	std::cout << "用户电话: " << user_phone << std::endl;
	std::cout << "收货地址: " << shipping_address << std::endl;
	std::cout << "优惠策略: " << discount_policy << std::endl;
	std::cout << "总金额: " << total_amount << std::endl;
	std::cout << "优惠金额: " << discount_amount << std::endl;
	std::cout << "实付金额: " << final_amount << std::endl;
	std::cout << "商品列表:" << std::endl;
	for (const auto& item : items) {
		std::cout << "  商品ID: " << item.good_id
				  << ", 商品名称: " << item.good_name
				  << ", 价格: " << item.price
				  << ", 数量: " << item.quantity
				  << ", 小计: " << item.subtotal
				  << std::endl;
	}
}

double TemporaryCart::totalPrice() const {
	double total = 0.0;
	for (const auto& item : items) {
		total += item.subtotal;
	}
	return total;
}

void TemporaryCart::clearGoods() {
	items.clear();
	total_amount = 0.0;
	discount_amount = 0.0;
	final_amount = 0.0;
	is_converted = false;
}