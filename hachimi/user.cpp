#include "user.h"

// 实现默认构造函数
User::User()
    : phone(""), password(""), address(""), cart(), orders(), orderItems() {
}

// 正确实现构造函数
User::User(std::string phone, std::string password, std::string address)
    : phone(phone), password(password), address(address) {}

// 正确实现成员函数
std::string User::getPhone() const{ return phone; }
std::string User::getPassword() const{ return password; }
std::string User::getAddress() const{ return address; }
void User::setPassword(std::string newpassword) { password = newpassword; }
void User::setAddress(std::string newaddress) { address = newaddress; }
bool User::isValidPhoneNumber(const std::string& phone) {
    if (phone.length() != 11) return false;
    return std::all_of(phone.begin(), phone.end(), ::isdigit);
}
// cart
TemporaryCart& User::getCart() { return cart; }
void User::showCart() const { cart.showGoods(); }

// order
std::vector<Order>& User::getOrders() { return orders; }
void User::addOrder(const Order& order) { orders.push_back(order); }
void User::showOrders() const {
    for (const auto& order : orders) {
        order.showOrder();
        std::cout << "------------------------" << std::endl;
    }
}
std::vector<OrderItem>& User::getOrderItems() { return orderItems; }
void User::addOrderItem(const OrderItem& item) { orderItems.push_back(item); }
void User::showOrderItems() const {
    for (const auto& item : orderItems) {
        std::cout << "订单ID: " << item.getOrderId()
                  << ", 商品ID: " << item.getGoodId()
                  << ", 商品名称: " << item.getGoodName()
                  << ", 价格: " << item.getPrice()
                  << ", 数量: " << item.getQuantity()
                  << ", 小计: " << item.getSubtotal()
                  << std::endl;
    }
}