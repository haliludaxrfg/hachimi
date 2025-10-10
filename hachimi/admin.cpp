#include "admin.h"
#include <iostream>

const std::string Admin::password = "CCBC4nmb"; // 默认管理员密码
Admin::Admin(DatabaseManager* db) : db(db) {
	loadAllData();
}

// 用户管理
bool Admin::addUser(const User& user) {
	if (db->DTBaddUser(user)) {
		users.push_back(user);
		return true;
	}
	return false;
}

bool Admin::updateUser(const User& user) {
    User* u = findUser(user.getPhone());
    if (!u) {
        std::cout << "未找到该用户！" << std::endl;
        return false;
    }
    std::string newPhone = u->getPhone();
    std::string newPassword = u->getPassword();
    std::string newAddress = u->getAddress();
    std::string input;
    std::cout << "当前手机号: " << newPhone << "，输入新手机号(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") newPhone = input;
    std::cout << "当前密码: " << newPassword << "，输入新密码(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") newPassword = input;
    std::cout << "当前地址: " << newAddress << "，输入新地址(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") newAddress = input;
    std::cout << "确认修改？(y/n): ";
    std::getline(std::cin, input);
    if (input != "y" && input != "Y") {
        std::cout << "操作已取消。" << std::endl;
        return false;
    }
    User newUser(newPhone, newPassword, newAddress);
    if (db->DTBupdateUser(newUser)) {
        *u = newUser;
        std::cout << "用户信息已更新！" << std::endl;
        return true;
    } else {
        std::cout << "数据库更新失败！" << std::endl;
        return false;
    }
}

bool Admin::removeUser(const std::string& phone) {
    std::cout << "确定要删除手机号为 " << phone << " 的用户吗？(y/n): ";
    char confirm;
    std::cin >> confirm;
    if (confirm != 'y' && confirm != 'Y') {
        std::cout << "操作已取消。" << std::endl;
        return false;
    }
    auto it = std::remove_if(users.begin(), users.end(),
        [&phone](const User& u) { return u.getPhone() == phone; });
    if (it != users.end()) {
        if (db->DTBdeleteUser(phone)) {
            users.erase(it, users.end());
            return true;
        }
    }
    return false;
}

User *Admin::findUser(const std::string& phone) {
    for (auto& user : users) {
        if (user.getPhone() == phone) {
            return &user;
        }
    }
    return nullptr;
}

std::vector<User>& Admin::getAllUsers() {
    return users;
}

// 商品管理
bool Admin::addGood(const Good& good) {
    if (db->DTBsaveGood(good)) {
        goods.push_back(good);
        return true;
    }
    return false;
}

bool Admin::updateGood(const Good& good) {
    Good* g = nullptr;
    for (auto& item : goods) {
        if (item.getId() == good.getId()) {
            g = &item;
            break;
        }
    }
    if (!g) {
        std::cout << "未找到该商品！" << std::endl;
        return false;
    }
    std::string newName = g->getName();
    double newPrice = g->getPrice();
    int newStock = g->getStock();
    std::string newCategory = g->getCategory();
    std::string input;
    std::cout << "当前商品名: " << newName << "，输入新商品名(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") newName = input;
    std::cout << "当前价格: " << newPrice << "，输入新价格(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") {
        try {
            newPrice = std::stod(input);
        } catch (...) {
            std::cout << "价格输入无效，保持原价。" << std::endl;
        }
    }
    std::cout << "当前库存: " << newStock << "，输入新库存(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") {
        try {
            newStock = std::stoi(input);
        } catch (...) {
            std::cout << "库存输入无效，保持原库存。" << std::endl;
        }
    }
    std::cout << "当前分类: " << newCategory << "，输入新分类(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") newCategory = input;
    std::cout << "确认修改？(y/n): ";
    std::getline(std::cin, input);
    if (input != "y" && input != "Y") {
        std::cout << "操作已取消。" << std::endl;
        return false;
    }
    Good newGood(g->getId(), newName, newPrice, newStock, newCategory);
    if (db->DTBupdateGood(newGood)) {
        *g = newGood;
        std::cout << "商品信息已更新！" << std::endl;
        return true;
    } else {
        std::cout << "数据库更新失败！" << std::endl;
        return false;
    }
}

bool Admin::removeGood(int good_id) {
    std::cout << "确定要删除商品ID为 " << good_id << " 的商品吗？(y/n): ";
    char confirm;
    std::cin >> confirm;
    if (confirm != 'y' && confirm != 'Y') {
        std::cout << "操作已取消。" << std::endl;
        return false;
    }
    auto it = std::remove_if(goods.begin(), goods.end(),
        [good_id](const Good& g) { return g.getId() == good_id; });
    if (it != goods.end()) {
        if (db->DTBdeleteGood(good_id)) {
            goods.erase(it, goods.end());
            return true;
        }
    }
    return false;
}

Good* Admin::findGood(int good_id) {
    for (auto& good : goods) {
        if (good.getId() == good_id) {
            return &good;
        }
    }
    return nullptr;
}

std::vector<Good>& Admin::getAllGoods() {
    return goods;
}

// 订单管理
bool Admin::addOrder(const Order& order) {
    if (db->DTBsaveOrder(order)) {
        orders.push_back(order);
        return true;
    }
    return false;
}

bool Admin::updateOrder(const Order& order) {
    Order* o = nullptr;
    for (auto& item : orders) {
        if (item.getOrderId() == order.getOrderId()) {
            o = &item;
            break;
        }
    }
    if (!o) {
        std::cout << "未找到该订单！" << std::endl;
        return false;
    }
    int newStatus = o->getStatus();
    std::string newShippingAddress = o->getShippingAddress();
    std::string newDiscountPolicy = o->getDiscountPolicy();
    std::string input;
    std::cout << "当前订单状态: " << newStatus << "，输入新状态(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") {
        try {
            newStatus = std::stoi(input);
        } catch (...) {
            std::cout << "状态输入无效，保持原状态。" << std::endl;
        }
    }
    std::cout << "当前收货地址: " << newShippingAddress << "，输入新地址(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") newShippingAddress = input;
    std::cout << "当前优惠策略: " << newDiscountPolicy << "，输入新策略(或q跳过): ";
    std::getline(std::cin, input);
    if (!input.empty() && input != "q") newDiscountPolicy = input;
    std::cout << "确认修改？(y/n): ";
    std::getline(std::cin, input);
    if (input != "y" && input != "Y") {
        std::cout << "操作已取消。" << std::endl;
        return false;
    }
    o->setStatus(newStatus);
    o->setShippingAddress(newShippingAddress);
    o->setDiscountPolicy(newDiscountPolicy);
    if (db->DTBupdateOrder(*o)) {
        std::cout << "订单信息已更新！" << std::endl;
        return true;
    } else {
        std::cout << "数据库更新失败！" << std::endl;
        return false;
    }
}

bool Admin::removeOrder(const std::string& order_id) {
    std::cout << "确定要删除订单ID为 " << order_id << " 的订单吗？(y/n): ";
    char confirm;
    std::cin >> confirm;
    if (confirm != 'y' && confirm != 'Y') {
        std::cout << "操作已取消。" << std::endl;
        return false;
    }
    auto it = std::remove_if(orders.begin(), orders.end(),
        [&order_id](const Order& o) { return o.getOrderId() == order_id; });
    if (it != orders.end()) {
        if (db->DTBdeleteOrder(order_id)) {
            orders.erase(it, orders.end());
            return true;
        }
    }
    return false;
}

Order* Admin::findOrder(const std::string& order_id) {
    for (auto& order : orders) {
        if (order.getOrderId() == order_id) {
            return &order;
        }
    }
    return nullptr;
}

std::vector<Order>& Admin::getAllOrders() {
    return orders;
}

// 数据同步
void Admin::loadAllData() {
    users = db->DTBloadAllUsers();
    goods = db->DTBloadAllGoods();
    orders = db->DTBloadRecentOrders();
}

void Admin::saveAllData() {
    for (const auto& user : users) {
        db->DTBsaveUser(user);
    }
    for (const auto& good : goods) {
        db->DTBsaveGood(good);
    }
    for (const auto& order : orders) {
        db->DTBsaveOrder(order);
    }
}