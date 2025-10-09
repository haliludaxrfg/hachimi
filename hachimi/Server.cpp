#include "Server.h"
#include "QtServer.h"



Server::Server(int port)
    : port(port), server(nullptr)
{
    dbManager = new DatabaseManager("localhost", "root", "a5B3#eF7hJ", "remake", 3306);
    if (!dbManager->initialize()) {
        std::cerr << "数据库连接失败！" << std::endl;
    } else {
        std::cout << "数据库连接成功！" << std::endl;
    }
}

Server::~Server() {
    if (server) {
        server->close();
        delete server;
        server = nullptr;
    }
    if (dbManager) {
        delete dbManager;
        dbManager = nullptr;
    }
}

void Server::start() {
    Logger::instance().info("服务器正在启动，监听端口: " + std::to_string(port));
    if (!server) {
        server = new QtServer(this);
        if (!server->listen(QHostAddress::Any, port)) {
            Logger::instance().info("服务器监听失败");
        } else {
            Logger::instance().info("服务器已监听端口: " + std::to_string(port));
        }
    }
}

void Server::stop() {
    std::cout << "服务器停止" << std::endl;
    if (server) {
        server->close();
    }
}

std::string Server::processRequest(const std::string& request) {
    return "别急";
}
/*
std::string Server::processRequest(const std::string& request) {
    // 简单的协议解析，假设请求格式为 "COMMAND|param1|param2|..."
    std::istringstream ss(request);
    std::string command;
    std::getline(ss, command, '|');

    // 去除命令前后空格
    command.erase(0, command.find_first_not_of(" \t\r\n"));
    command.erase(command.find_last_not_of(" \t\r\n") + 1);

    if (command == "GET_ALL_GOODS") {
        return getAllGoods();
    } else if (command == "UPDATE_GOODS") {
        int id, stock;
        std::string name, category;
        double price;
        std::getline(ss, name, '|');
        ss >> id;
        ss.ignore(1);
        ss >> price;
        ss.ignore(1);
        ss >> stock;
        ss.ignore(1);
        std::getline(ss, category, '|');
        return updateGoods(id, name, price, stock, category);
    } else if (command == "GET_ALL_ACCOUNTS") {
        return getAllAccounts();
    } else if (command == "LOGIN") {
        std::string phone, password;
        std::getline(ss, phone, '|');
        std::getline(ss, password, '|');
        return login(phone, password);
    } else if (command == "UPDATE_ACCOUNT_PASSWORD") {
        std::string userId, oldPassword, newPassword;
        std::getline(ss, userId, '|');
        std::getline(ss, oldPassword, '|');
        std::getline(ss, newPassword, '|');
        return updateAccountPassword(userId, oldPassword, newPassword);
    } else if (command == "DELETE_ACCOUNT") {
        std::string phone, password;
        std::getline(ss, phone, '|');
        std::getline(ss, password, '|');
        return deleteAccount(phone, password);
    } else if (command == "ADD_ACCOUNT") {
        std::string phone, password;
        std::getline(ss, phone, '|');
        std::getline(ss, password, '|');
        return addAccount(phone, password);
    } else if (command == "GET_ALL_ORDERS") {
        std::string userPhone;
        std::getline(ss, userPhone, '|');
        return getAllOrders(userPhone);
    } else if (command == "GET_ORDER_DETAIL") {
        std::string orderId, userPhone;
        std::getline(ss, orderId, '|');
        std::getline(ss, userPhone, '|');
        return getOrderDetail(orderId, userPhone);
    } else if (command == "UPDATE_ORDER_STATUS") {
        std::string orderId, userPhone;
        int newStatus;
        std::getline(ss, orderId, '|');
        std::getline(ss, userPhone, '|');
        ss >> newStatus;
        return updateOrderStatus(orderId, userPhone, newStatus);
    } else if (command == "ADD_SETTLED_ORDER") {
        std::string orderId, productName, userPhone, discountPolicy;
        int productId, quantity, status;
        std::getline(ss, orderId, '|');
        std::getline(ss, productName, '|');
        ss >> productId;
        ss.ignore(1);
        ss >> quantity;
        ss.ignore(1);
        std::getline(ss, userPhone, '|');
        ss >> status;
        ss.ignore(1);
        std::getline(ss, discountPolicy, '|');
        return addSettledOrder(orderId, productName, productId, quantity, userPhone, status, discountPolicy);
    } else if (command == "RETURN_SETTLED_ORDER") {
        std::string orderId, userPhone;
        std::getline(ss, orderId, '|');
        std::getline(ss, userPhone, '|');
        return returnSettledOrder(orderId, userPhone);
    } else if (command == "REPAIR_SETTLED_ORDER") {
        std::string orderId, userPhone;
        std::getline(ss, orderId, '|');
        std::getline(ss, userPhone, '|');
        return repairSettledOrder(orderId, userPhone);
    } else if (command == "DELETE_SETTLED_ORDER") {
        std::string orderId, userPhone;
        std::getline(ss, orderId, '|');
        std::getline(ss, userPhone, '|');
        return deleteSettledOrder(orderId, userPhone);
    } else if (command == "GET_CART") {
        std::string userPhone;
        std::getline(ss, userPhone, '|');
        return getCart(userPhone);
    } else if (command == "SAVE_CART") {
        std::string userPhone, cartData;
        std::getline(ss, userPhone, '|');
        std::getline(ss, cartData, '|');
        return saveCart(userPhone, cartData);
    } else if (command == "ADD_TO_CART") {
        std::string userPhone, productName;
        int productId, quantity;
        double price;
        std::getline(ss, userPhone, '|');
        ss >> productId;
        ss.ignore(1);
        std::getline(ss, productName, '|');
        ss >> price;
        ss.ignore(1);
        ss >> quantity;
        return addToCart(userPhone, productId, productName, price, quantity);
    } else if (command == "UPDATE_CART_ITEM") {
        std::string userPhone;
        int productId, quantity;
        std::getline(ss, userPhone, '|');
        ss >> productId;
        ss.ignore(1);
        ss >> quantity;
        return updateCartItem(userPhone, productId, quantity);
    } else if (command == "REMOVE_FROM_CART") {
        std::string userPhone;
        int productId;
        std::getline(ss, userPhone, '|');
        ss >> productId;
        return removeFromCart(userPhone, productId);
    } else if (command == "UPDATE_GOOD") {
        int goodId, stock;
        std::string name;
        double price;
        ss >> goodId;
        ss.ignore(1);
        std::getline(ss, name, '|');
        ss >> price;
        ss.ignore(1);
        ss >> stock;
        return updateGood(goodId, name, price, stock);
    } else if (command == "GET_ALL_PROMOTIONS") {
        return getAllPromotions();
    } else if (command == "GET_PROMOTIONS_BY_PRODUCT_ID") {
        int productId;
        ss >> productId;
        return getPromotionsByProductId(productId);
    } else if (command == "UPDATE_CART_FOR_PROMOTIONS") {
        std::string userPhone;
        std::getline(ss, userPhone, '|');
        return updateCartForPromotions(userPhone);
    }
    return "UNKNOWN_COMMAND";
}
*/

