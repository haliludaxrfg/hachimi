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

// 非交互式：使用传入的 user 对象直接更新数据库与内存缓存
bool Admin::updateUser(const User& user) {
    User* u = findUser(user.getPhone());
    if (!u) {
        std::cout << "未找到该用户: " << user.getPhone() << std::endl;
        return false;
    }
    if (!db->DTBupdateUser(user)) {
        std::cout << "数据库更新失败: " << user.getPhone() << std::endl;
        return false;
    }
    *u = user; // 更新内存缓存
    return true;
}

// 非交互式：直接删除指定手机号的用户
bool Admin::removeUser(const std::string& phone) {
    auto it = std::remove_if(users.begin(), users.end(),
        [&phone](const User& u) { return u.getPhone() == phone; });
    if (it != users.end()) {
        if (db->DTBdeleteUser(phone)) {
            users.erase(it, users.end());
            return true;
        } else {
            std::cout << "数据库删除失败: " << phone << std::endl;
            return false;
        }
    }
    // 若内存中不存在仍尝试删除数据库记录
    if (db->DTBdeleteUser(phone)) return true;
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

// 非交互式：更新指定 Good（以 good.getId() 为主键）
bool Admin::updateGood(const Good& good) {
    Good* g = nullptr;
    for (auto& item : goods) {
        if (item.getId() == good.getId()) {
            g = &item;
            break;
        }
    }
    if (!g) {
        std::cout << "未找到该商品 id=" << good.getId() << std::endl;
        return false;
    }
    if (!db->DTBupdateGood(good)) {
        std::cout << "数据库更新失败: good id=" << good.getId() << std::endl;
        return false;
    }
    *g = good;
    return true;
}

// 非交互式：删除商品
bool Admin::removeGood(int good_id) {
    auto it = std::remove_if(goods.begin(), goods.end(),
        [good_id](const Good& g) { return g.getId() == good_id; });
    if (it != goods.end()) {
        if (db->DTBdeleteGood(good_id)) {
            goods.erase(it, goods.end());
            return true;
        } else {
            std::cout << "数据库删除失败: good id=" << good_id << std::endl;
            return false;
        }
    }
    // 若内存中不存在仍尝试删除数据库记录
    if (db->DTBdeleteGood(good_id)) return true;
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

// 非交互式：以传入 order 覆盖更新
bool Admin::updateOrder(const Order& order) {
    Order* o = nullptr;
    for (auto& item : orders) {
        if (item.getOrderId() == order.getOrderId()) {
            o = &item;
            break;
        }
    }
    if (!o) {
        std::cout << "未找到该订单 id=" << order.getOrderId() << std::endl;
        return false;
    }
    if (!db->DTBupdateOrder(order)) {
        std::cout << "数据库更新失败: order id=" << order.getOrderId() << std::endl;
        return false;
    }
    *o = order;
    return true;
}

// 非交互式：删除订单
bool Admin::removeOrder(const std::string& order_id) {
    auto it = std::remove_if(orders.begin(), orders.end(),
        [&order_id](const Order& o) { return o.getOrderId() == order_id; });
    if (it != orders.end()) {
        if (db->DTBdeleteOrder(order_id)) {
            orders.erase(it, orders.end());
            return true;
        } else {
            std::cout << "数据库删除失败: order id=" << order_id << std::endl;
            return false;
        }
    }
    // 若内存中不存在仍尝试删除数据库记录
    if (db->DTBdeleteOrder(order_id)) return true;
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