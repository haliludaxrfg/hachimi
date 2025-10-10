#pragma once
#include <string>
#include <vector>
#include "TemporaryCart.h"
#include "good.h"
#include "cartItem.h"
#include "order.h"
#include "orderItem.h"

class User {

	std::string phone;//(11位)==id
	std::string password; //5-25
	std::string address;
	TemporaryCart cart; // 新增：购物车
	std::vector<Order> orders; // 新增：订单列表
	std::vector<OrderItem> orderItems; // 新增：订单项列表
public:
	User(std::string phone, std::string password, std::string address);
	// 基本信息
	std::string getPhone()const;
	std::string getPassword()const;
	std::string getAddress()const;
	void setPassword(std::string newpassword);
	void setAddress(std::string newaddress);


	// 购物车相关
	TemporaryCart& getCart();
	void showCart() const;

	// 订单相关
	std::vector<Order>& getOrders();
	void addOrder(const Order& order);
	void showOrders() const;
	std::vector<OrderItem>& getOrderItems();
	void addOrderItem(const OrderItem& item);
	void showOrderItems() const;

	// 静态方法
	static bool isValidPhoneNumber(const std::string& phone);
};
