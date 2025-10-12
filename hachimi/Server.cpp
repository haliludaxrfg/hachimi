#include "Server.h"
#include "logger.h"
#include <QHostAddress>
#include <nlohmann/json.hpp>

#include <chrono>
#include <random>
#include <sstream>

using nlohmann::json;

Server::Server(int port)
    : port(port), server(nullptr)
{
    dbManager = new DatabaseManager("127.0.0.1", "root", "a5B3#eF7hJ", "remake", 3306);
    if (!dbManager->DTBinitialize()) {
        Logger::instance().fail("Server: DatabaseManager 初始化失败，DTBconnect 返回 false");
    } else {
        Logger::instance().info("Server: DatabaseManager 已连接。");
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

std::string Server::generateCartId(const std::string& userPhone) {
    // 格式: c<timestamp_ms>_<hex-rand>
    using namespace std::chrono;
    uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    // 线程局部随机数引擎
    static thread_local std::mt19937_64 rng((std::random_device())());
    std::uniform_int_distribution<uint64_t> dist(0, std::numeric_limits<uint64_t>::max());
    uint64_t r = dist(rng);

    std::ostringstream oss;
    oss << "c" << ms << "_" << std::hex << r;
    std::string id = oss.str();

    // 确保符合 VARCHAR(64)
    if (id.size() > 63) id.resize(63);
    return id;
}

void Server::SERstart() {
    if (!server) {
        server = new QTcpServer();
        // 只监听127.0.0.1:8888
        if (!server->listen(QHostAddress("127.0.0.1"), port)) {
            qDebug() << "服务器监听失败";
            return;
        }
        qDebug() << "服务器已监听端口:" << port << " 地址: 127.0.0.1";

        QObject::connect(server, &QTcpServer::newConnection, [this]() {
            while (server->hasPendingConnections()) {
                QTcpSocket* clientSocket = server->nextPendingConnection();
                QObject::connect(clientSocket, &QTcpSocket::readyRead, [this, clientSocket]() {
                    QByteArray data = clientSocket->readAll();
                    // Log raw received data for debugging
                    qDebug() << "Server received raw:" << data;
                    QString reqStr = QString::fromUtf8(data).trimmed();
                    qDebug() << "Server processed request string:" << reqStr;
                    std::string response = SERprocessRequest(reqStr.toStdString());
                    // write back response (may be empty)
                    QByteArray out = QByteArray::fromStdString(response);
                    clientSocket->write(out);
                    clientSocket->flush();
                });
                QObject::connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
            }
        });
    }
}

void Server::SERstop() {
    if (server) {
        server->close();
    }
}

std::string Server::SERprocessRequest(const std::string& request) {
    auto trim = [](std::string s) {
        const char* ws = " \t\n\r";
        s.erase(0, s.find_first_not_of(ws));
        s.erase(s.find_last_not_of(ws) + 1);
        return s;
        };

    try {
        std::string cmd;
        std::string rest;
        auto pos = request.find(' ');
        if (pos == std::string::npos) {
            cmd = trim(request);
        }
        else {
            cmd = trim(request.substr(0, pos));
            rest = trim(request.substr(pos + 1));
        }

        // 简单命令
        if (cmd == "socket_test_hello") {
            return "socket_test_hello";
        }
        if (cmd == "GET_ALL_GOODS") {
            Logger::instance().info("Server 收到请求: GET_ALL_GOODS");
            return SERgetAllGoods();
        }
        if (cmd == "GET_ALL_ACCOUNTS") {
            Logger::instance().info("Server 收到请求: GET_ALL_ACCOUNTS");
            return SERgetAllAccounts();
        }

        // Helper to parse JSON payload if provided
        auto parseJson = [&](const std::string& s) -> nlohmann::json {
            if (s.empty()) return nlohmann::json();
            if (s.front() == '{' || s.front() == '[') return nlohmann::json::parse(s);
            return nlohmann::json();
            };

        // ----- 商品相关 -----
        if (cmd == "GET_GOOD_BY_ID") {
            // rest 可以是单个 id 或 json {"id":123}
            try {
                if (rest.empty()) return "MISSING_PARAMETER";
                if (rest.front() == '{') {
                    auto j = parseJson(rest);
                    return SERgetGoodById(j.value("id", 0));
                }
                else {
                    int id = std::stoi(rest);
                    return SERgetGoodById(id);
                }
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "ADD_GOOD") {
            // rest 应为 json {"name":"n","price":1.2,"stock":10,"category":"c"}
            try {
                auto j = parseJson(rest);
                if (j.is_object()) {
                    return SERaddGood(j.value("name", std::string("")),
                        j.value("price", 0.0),
                        j.value("stock", 0),
                        j.value("category", std::string("")));
                }
                nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump();
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "UPDATE_GOOD") {
            // rest json {"id":1,"name":"n",...}
            try {
                auto j = parseJson(rest);
                if (j.is_object()) {
                    return SERupdateGood(j.value("id", 0),
                        j.value("name", std::string("")),
                        j.value("price", 0.0),
                        j.value("stock", 0),
                        j.value("category", std::string("")));
                }
                nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump();
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "DELETE_GOOD") {
            try {
                if (rest.front() == '{') {
                    auto j = parseJson(rest);
                    return SERdeleteGood(j.value("id", 0));
                }
                else {
                    int id = std::stoi(rest);
                    return SERdeleteGood(id);
                }
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "SEARCH_GOODS_BY_CATEGORY") {
            // rest: category string or json {"category":"c"}
            if (rest.front() == '{') {
                auto j = parseJson(rest);
                return SERsearchGoodsByCategory(j.value("category", std::string("")));
            }
            else {
                return SERsearchGoodsByCategory(rest);
            }
        }

        // ----- 账号相关 -----
        if (cmd == "LOGIN") {
            try {
                if (rest.front() == '{') {
                    auto j = parseJson(rest);
                    return SERlogin(j.value("phone", std::string("")), j.value("password", std::string("")));
                }
                else {
                    std::istringstream iss(rest);
                    std::string phone, pwd;
                    iss >> phone >> pwd;
                    return SERlogin(phone, pwd);
                }
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "ADD_ACCOUNT") {
            try {
                auto j = parseJson(rest);
                if (j.is_object()) return SERaddAccount(j.value("phone", std::string("")), j.value("password", std::string("")),j.value("address",std::string("")));
                nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump();
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "UPDATE_ACCOUNT_PASSWORD") {
            try {
                auto j = parseJson(rest);
                if (j.is_object()) return SERupdateAccountPassword(j.value("userId", std::string("")), j.value("oldPassword", std::string("")), j.value("newPassword", std::string("")));
                nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump();
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "DELETE_ACCOUNT") {
            try {
                auto j = parseJson(rest);
                if (j.is_object()) return SERdeleteAccount(j.value("phone", std::string("")), j.value("password", std::string("")));
                nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump();
            }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "GET_ALL_ACCOUNTS") {
            return SERgetAllAccounts();
        }
        if (cmd == "UPDATE_USER") {
            try {
                auto j = parseJson(rest);
                if (!j.is_object()) { nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump(); }
                return SERupdateUser(j.value("phone", std::string("")), j.value("password", std::string("")), j.value("address", std::string("")));
            } catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }

        // ----- 订单相关 -----
        if (cmd == "GET_ALL_ORDERS") {
            // rest: userPhone or json {"userPhone":"..."}
            if (rest.front() == '{') {
                auto j = parseJson(rest);
                return SERgetAllOrders(j.value("userPhone", std::string("")));
            }
            else {
                return SERgetAllOrders(rest);
            }
        }
        if (cmd == "GET_ORDER_DETAIL") {
            // rest: "orderId userPhone" or json
            if (rest.front() == '{') {
                auto j = parseJson(rest);
                return SERgetOrderDetail(j.value("orderId", std::string("")), j.value("userPhone", std::string("")));
            }
            else {
                std::istringstream iss(rest); std::string orderId, userPhone; iss >> orderId >> userPhone;
                return SERgetOrderDetail(orderId, userPhone);
            }
        }
        if (cmd == "UPDATE_ORDER_STATUS") {
            // json required: {"orderId":"..","userPhone":"..","newStatus":2}
            auto j = parseJson(rest);
            return SERupdateOrderStatus(j.value("orderId", std::string("")), j.value("userPhone", std::string("")), j.value("newStatus", 0));
        }
        if (cmd == "ADD_SETTLED_ORDER") {
            // json required
            auto j = parseJson(rest);
            return SERaddSettledOrder(j.value("orderId", std::string("")),
                j.value("productName", std::string("")),
                j.value("productId", 0),
                j.value("quantity", 0),
                j.value("userPhone", std::string("")),
                j.value("status", 0),
                j.value("discountPolicy", std::string("")));
        }
        if (cmd == "RETURN_SETTLED_ORDER") {
            auto j = parseJson(rest);
            return SERreturnSettledOrder(j.value("orderId", std::string("")), j.value("userPhone", std::string("")));
        }
        if (cmd == "REPAIR_SETTLED_ORDER") {
            auto j = parseJson(rest);
            return SERrepairSettledOrder(j.value("orderId", std::string("")), j.value("userPhone", std::string("")));
        }
        if (cmd == "DELETE_SETTLED_ORDER") {
            auto j = parseJson(rest);
            return SERdeleteSettledOrder(j.value("orderId", std::string("")), j.value("userPhone", std::string("")));
        }

        // ----- 购物车相关 -----
        if (cmd == "GET_CART") {
            return SERgetCart(rest);
        }
        if (cmd == "SAVE_CART") {
            // rest: userPhone <jsonCart>  或 json {"userPhone":"..","cart":{...}}
            if (!rest.empty() && rest.front() == '{') {
                auto j = parseJson(rest);
                std::string phone = j.value("userPhone", std::string(""));
                if (!phone.empty() && j.contains("cart")) {
                    return SERsaveCart(phone, j["cart"].dump());
                }
                else if (!phone.empty() && j.contains("cartData")) {
                    return SERsaveCart(phone, j.value("cartData", std::string("")));
                }
                nlohmann::json e; e["error"] = "参数缺失"; return e.dump();
            }
            else {
                // split first token as phone, rest as json
                auto p2 = rest.find(' ');
                if (p2 == std::string::npos) return "MISSING_PARAMETERS";
                std::string phone = trim(rest.substr(0, p2));
                std::string cartJson = trim(rest.substr(p2 + 1));
                return SERsaveCart(phone, cartJson);
            }
        }
        if (cmd == "ADD_TO_CART") {
            // prefer json: {"userPhone":"..","productId":..,"productName":"..","price":..,"quantity":..}
            if (!rest.empty() && rest.front() == '{') {
                auto j = parseJson(rest);
                return SERaddToCart(j.value("userPhone", std::string("")),
                    j.value("productId", 0),
                    j.value("productName", std::string("")),
                    j.value("price", 0.0),
                    j.value("quantity", 0));
            }
            else {
                std::istringstream iss(rest);
                std::string phone, name; int id; double price; int qty;
                if (!(iss >> phone >> id >> price >> qty)) { nlohmann::json e; e["error"] = "参数格式错误"; return e.dump(); }
                // productName not supported in space-separated form
                return SERaddToCart(phone, id, std::string(""), price, qty);
            }
        }
        if (cmd == "UPDATE_CART_ITEM") {
            if (!rest.empty() && rest.front() == '{') {
                auto j = parseJson(rest);
                return SERupdateCartItem(j.value("userPhone", std::string("")), j.value("productId", 0), j.value("quantity", 0));
            }
            else {
                std::istringstream iss(rest); std::string phone; int pid, qty; iss >> phone >> pid >> qty;
                return SERupdateCartItem(phone, pid, qty);
            }
        }
        if (cmd == "REMOVE_FROM_CART") {
            if (!rest.empty() && rest.front() == '{') {
                auto j = parseJson(rest);
                return SERremoveFromCart(j.value("userPhone", std::string("")), j.value("productId", 0));
            }
            else {
                std::istringstream iss(rest); std::string phone; int pid; iss >> phone >> pid;
                return SERremoveFromCart(phone, pid);
            }
        }

        // promotion - not implemented but allow calls
        if (cmd == "GET_ALL_PROMOTIONS") return SERgetAllPromotions();
        if (cmd == "GET_PROMOTIONS_BY_PRODUCT_ID") {
            try { int pid = std::stoi(rest); return SERgetPromotionsByProductId(pid); }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "UPDATE_CART_FOR_PROMOTIONS") return SERupdateCartForPromotions(rest);

        Logger::instance().warn("Server 收到未知请求: " + request);
        nlohmann::json r; r["error"] = "UNKNOWN_COMMAND"; r["request"] = request;
        return r.dump();
    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server 处理请求异常: ") + e.what() + ", 请求内容: " + request);
        nlohmann::json r; r["error"] = "exception"; r["message"] = e.what(); return r.dump();
    }
    catch (...) {
        Logger::instance().fail("Server 处理请求发生未知异常, 请求内容: " + request);
        nlohmann::json r; r["error"] = "unknown_exception"; return r.dump();
    }
}

std::string Server::SERgetAllGoods() {
    if (!dbManager) {
        Logger::instance().fail("Server SERgetAllGoods: dbManager is null");
        nlohmann::json errorResponse; errorResponse["error"] = "服务器内部错误"; errorResponse["message"] = "dbManager 为空";
        return errorResponse.dump();
    }
    if (!dbManager->DTBisConnected()) {
        Logger::instance().warn("Server SERgetAllGoods: dbManager 未连接，尝试重新连接...");
        if (!dbManager->DTBinitialize()) {
            Logger::instance().fail("Server SERgetAllGoods: 重新连接数据库失败");
            nlohmann::json errorResponse; errorResponse["error"] = "数据库未连接"; errorResponse["message"] = "无法连接数据库";
            return errorResponse.dump();
        }
    }

    Logger::instance().info(std::string("Server SERgetAllGoods: dbManager ptr = ") +
        std::to_string(reinterpret_cast<uintptr_t>(reinterpret_cast<void*>(dbManager))) +
        ", DTBisConnected=" + (dbManager->DTBisConnected() ? "true" : "false"));

    try {
        std::vector<Good> goods = dbManager->DTBloadAllGoods();
        if (goods.empty()) {
            Logger::instance().warn("Server SERgetAllGoods: 查询结果为空，返回提示信息。");
            nlohmann::json msg;
            msg["error"] = "目前没有商品！";
            Logger::instance().info("Server SERgetAllGoods: 返回JSON内容: " + msg.dump());
            return msg.dump();
        }
        nlohmann::json j = nlohmann::json::array();
        for (const auto& g : goods) {
            j.push_back({
                {"id", g.getId()},
                {"name", g.getName()},
                {"price", g.getPrice()},
                {"stock", g.getStock()},
                {"category", g.getCategory()}
                });
        }
        std::string jsonStr = j.dump();
        Logger::instance().info("Server SERgetAllGoods: 返回JSON内容: " + jsonStr);
        Logger::instance().info("Server SERgetAllGoods: 查询到 " + std::to_string(goods.size()) + " 个商品");
        return jsonStr;
    }
    catch (const std::exception& e) {
        nlohmann::json errorResponse;
        errorResponse["error"] = "查询商品失败";
        errorResponse["message"] = e.what();
        Logger::instance().fail(std::string("Server SERgetAllGoods 异常: ") + e.what());
        Logger::instance().info("Server SERgetAllGoods: 返回JSON内容: " + errorResponse.dump());
        return errorResponse.dump();
    }
    catch (...) {
        nlohmann::json errorResponse;
        errorResponse["error"] = "查询商品失败";
        errorResponse["message"] = "未知异常";
        Logger::instance().fail("Server SERgetAllGoods 发生未知异常。");
        Logger::instance().info("Server SERgetAllGoods: 返回JSON内容: " + errorResponse.dump());
        return errorResponse.dump();
    }
}

std::string Server::SERupdateGood(int GoodId, std::string Goodname, double Goodprice, int Goodstock, std::string Goodcategory) {
    if (!dbManager) {
        Logger::instance().fail("Server SERupdateGoods: dbManager is null");
        nlohmann::json err; err["error"] = "服务器内部错误"; err["message"] = "dbManager 为空";
        return err.dump();
    }
    if (!dbManager->DTBisConnected()) {
        Logger::instance().warn("Server SERupdateGoods: dbManager 未连接，尝试重新连接...");
        if (!dbManager->DTBinitialize()) {
            Logger::instance().fail("Server SERupdateGoods: 重新连接数据库失败");
            nlohmann::json err; err["error"] = "数据库未连接"; err["message"] = "无法连接数据库";
            return err.dump();
        }
    }

    try {
        // 使用与 DatabaseManager 兼容的 Good 构造
        Good g(GoodId, Goodname, Goodprice, Goodstock, Goodcategory);
        bool ok = dbManager->DTBupdateGood(g);
        if (!ok) {
            Logger::instance().fail("Server SERupdateGoods: 更新商品失败，DB 返回 false");
            nlohmann::json res; res["error"] = "更新失败"; res["message"] = "数据库更新操作失败";
            Logger::instance().info("Server SERupdateGoods: 返回JSON内容: " + res.dump());
            return res.dump();
        }
        nlohmann::json j;
        j["id"] = g.getId();
        j["name"] = g.getName();
        j["price"] = g.getPrice();
        j["stock"] = g.getStock();
        j["category"] = g.getCategory();
        std::string jsonStr = j.dump();
        Logger::instance().info("Server SERupdateGoods: 更新成功，返回JSON内容: " + jsonStr);
        // 可选写入本地文件以便调试
        std::ofstream ofs("localJSONOutPut.json", std::ios::out | std::ios::trunc);
        if (ofs.is_open()) {
            ofs << jsonStr;
            ofs.close();
            Logger::instance().info("Server SERupdateGoods: 已写入 localJSONOutPut.json");
        }
        return jsonStr;
    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERupdateGoods 异常: ") + e.what());
        nlohmann::json err; err["error"] = "更新商品失败"; err["message"] = e.what();
        return err.dump();
    }
    catch (...) {
        Logger::instance().fail("Server SERupdateGoods 发生未知异常。");
        nlohmann::json err; err["error"] = "更新商品失败"; err["message"] = "未知异常";
        return err.dump();
    }
}

std::string Server::SERaddGood(const std::string& name, double price, int stock, const std::string& category) {
    if (!dbManager) {
        Logger::instance().fail("Server SERaddGood: dbManager is null");
        nlohmann::json err; err["error"] = "服务器内部错误"; err["message"] = "dbManager 为空";
        return err.dump();
    }
    if (!dbManager->DTBisConnected()) {
        Logger::instance().warn("Server SERaddGood: dbManager 未连接，尝试重新连接...");
        if (!dbManager->DTBinitialize()) {
            Logger::instance().fail("Server SERaddGood: 重新连接数据库失败");
            nlohmann::json err; err["error"] = "数据库未连接"; err["message"] = "无法连接数据库";
            return err.dump();
        }
    }

    try {
        Good g(0, name, price, stock, category);
        bool ok = dbManager->DTBsaveGood(g);
        if (!ok) {
            Logger::instance().fail("Server SERaddGood: 添加商品失败，DB 返回 false");
            nlohmann::json res; res["error"] = "添加失败"; res["message"] = "数据库添加操作失败";
            Logger::instance().info("Server SERaddGood: 返回JSON内容: " + res.dump());
            return res.dump();
        }
        nlohmann::json j;
        j["id"] = g.getId();
        j["name"] = g.getName();
        j["price"] = g.getPrice();
        j["stock"] = g.getStock();
        j["category"] = g.getCategory();
        std::string jsonStr = j.dump();
        Logger::instance().info("Server SERaddGood: 添加成功，返回JSON内容: " + jsonStr);
        std::ofstream ofs("localJSONOutPut.json", std::ios::out | std::ios::trunc);
        if (ofs.is_open()) {
            ofs << jsonStr;
            ofs.close();
            Logger::instance().info("Server SERaddGood: 已写入 localJSONOutPut.json");
        }
        return jsonStr;
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERaddGood 异常: ") + e.what());
        nlohmann::json err; err["error"] = "添加商品失败"; err["message"] = e.what();
        return err.dump();
    } catch (...) {
        Logger::instance().fail("Server SERaddGood 发生未知异常。");
        nlohmann::json err; err["error"] = "添加商品失败"; err["message"] = "未知异常";
        return err.dump();
    }
}

std::string Server::SERgetGoodById(int id) {
    if (!dbManager) {
        Logger::instance().fail("Server SERgetGoodById: dbManager is null");
        nlohmann::json err; err["error"] = "服务器内部错误"; err["message"] = "dbManager 为空";
        return err.dump();
    }
    if (!dbManager->DTBisConnected()) {
        Logger::instance().warn("Server SERgetGoodById: dbManager 未连接，尝试重新连接...");
        if (!dbManager->DTBinitialize()) {
            Logger::instance().fail("Server SERgetGoodById: 重新连接数据库失败");
            nlohmann::json err; err["error"] = "数据库未连接"; err["message"] = "无法连接数据库";
            return err.dump();
        }
    }

    try {
        Good g;
        if (!dbManager->DTBloadGood(id, g)) {
            nlohmann::json res; res["error"] = "未找到商品"; res["id"] = id;
            Logger::instance().warn("Server SERgetGoodById: 未找到商品 id=" + std::to_string(id));
            return res.dump();
        }
        nlohmann::json j;
        j["id"] = g.getId();
        j["name"] = g.getName();
        j["price"] = g.getPrice();
        j["stock"] = g.getStock();
        j["category"] = g.getCategory();
        std::string jsonStr = j.dump();
        Logger::instance().info("Server SERgetGoodById: 返回JSON内容: " + jsonStr);
        // 写入本地文件以便调试
        std::ofstream ofs("localJSONOutPut.json", std::ios::out | std::ios::trunc);
        if (ofs.is_open()) {
            ofs << jsonStr;
            ofs.close();
            Logger::instance().info("Server SERgetGoodById: 已写入 localJSONOutPut.json");
        }
        return jsonStr;
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERgetGoodById 异常: ") + e.what());
        nlohmann::json err; err["error"] = "查询商品失败"; err["message"] = e.what();
        return err.dump();
    } catch (...) {
        Logger::instance().fail("Server SERgetGoodById 发生未知异常。");
        nlohmann::json err; err["error"] = "查询商品失败"; err["message"] = "未知异常";
        return err.dump();
    }
}

std::string Server::SERdeleteGood(int id) {
    if (!dbManager) {
        Logger::instance().fail("Server SERdeleteGood: dbManager is null");
        nlohmann::json err; err["error"] = "服务器内部错误"; err["message"] = "dbManager 为空";
        return err.dump();
    }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) {
        Logger::instance().fail("Server SERdeleteGood: 数据库未连接");
        nlohmann::json err; err["error"] = "数据库未连接"; return err.dump();
    }
    try {
        if (!dbManager->DTBdeleteGood(id)) {
            nlohmann::json res; res["error"] = "删除失败"; res["id"] = id; return res.dump();
        }
        nlohmann::json ok; ok["result"] = "deleted"; ok["id"] = id;
        Logger::instance().info("Server SERdeleteGood: 删除商品 id=" + std::to_string(id));
        return ok.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERdeleteGood 异常: ") + e.what());
        nlohmann::json err; err["error"] = "删除失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERsearchGoodsByCategory(const std::string& category) {
    if (!dbManager) {
        Logger::instance().fail("Server SERsearchGoodsByCategory: dbManager is null");
        nlohmann::json err; err["error"] = "服务器内部错误"; return err.dump();
    }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) {
        Logger::instance().fail("Server SERsearchGoodsByCategory: 数据库未连接"); nlohmann::json err; err["error"] = "数据库未连接"; return err.dump();
    }
    try {
        auto goods = dbManager->DTBloadGoodsByCategory(category);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& g : goods) {
            arr.push_back({{"id", g.getId()}, {"name", g.getName()}, {"price", g.getPrice()}, {"stock", g.getStock()}, {"category", g.getCategory()}});
        }
        Logger::instance().info("Server SERsearchGoodsByCategory: 返回 " + std::to_string(goods.size()) + " 条");
        return arr.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERsearchGoodsByCategory 异常: ") + e.what()); nlohmann::json err; err["error"] = "查询失败"; err["message"] = e.what(); return err.dump();
    }
}

// ---------- User / Account APIs ----------
std::string Server::SERgetAllAccounts() {
    if (!dbManager) { Logger::instance().fail("Server SERgetAllAccounts: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERgetAllAccounts: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        auto users = dbManager->DTBloadAllUsers();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& u : users) {
            arr.push_back({{"phone", u.getPhone()}, {"password", u.getPassword()}, {"address", u.getAddress()}});
        }
        return arr.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERgetAllAccounts 异常: ") + e.what()); nlohmann::json err; err["error"] = "查询失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERlogin(const std::string& phone, const std::string& password) {
    if (!dbManager) { Logger::instance().fail("Server SERlogin: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERlogin: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        User u;
        if (!dbManager->DTBloadUser(phone, u)) {
            nlohmann::json r; r["error"] = "用户不存在"; return r.dump();
        }
        if (u.getPassword() != password) {
            nlohmann::json r; r["error"] = "密码错误"; return r.dump();
        }
        nlohmann::json ok; ok["result"] = "ok"; ok["phone"] = phone; return ok.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERlogin 异常: ") + e.what()); nlohmann::json err; err["error"] = "登录失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERupdateAccountPassword(const std::string& userId, const std::string& oldPassword, const std::string& newPassword) {
    if (!dbManager) { Logger::instance().fail("Server SERupdateAccountPassword: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERupdateAccountPassword: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        User u;
        if (!dbManager->DTBloadUser(userId, u)) { nlohmann::json r; r["error"] = "用户不存在"; return r.dump(); }
        if (u.getPassword() != oldPassword) { nlohmann::json r; r["error"] = "旧密码错误"; return r.dump(); }
        u.setPassword(newPassword);
        if (!dbManager->DTBupdateUser(u)) { nlohmann::json r; r["error"] = "更新失败"; return r.dump(); }
        nlohmann::json ok; ok["result"] = "updated"; ok["phone"] = userId; return ok.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERupdateAccountPassword 异常: ") + e.what()); nlohmann::json err; err["error"] = "更新失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERdeleteAccount(const std::string& phone, const std::string& password) {
    if (!dbManager) { Logger::instance().fail("Server SERdeleteAccount: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERdeleteAccount: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        User u;
        if (!dbManager->DTBloadUser(phone, u)) { nlohmann::json r; r["error"] = "用户不存在"; return r.dump(); }
        if (u.getPassword() != password) { nlohmann::json r; r["error"] = "密码错误"; return r.dump(); }
        if (!dbManager->DTBdeleteUser(phone)) { nlohmann::json r; r["error"] = "删除失败"; return r.dump(); }
        nlohmann::json ok; ok["result"] = "deleted"; ok["phone"] = phone; return ok.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERdeleteAccount 异常: ") + e.what()); nlohmann::json err; err["error"] = "删除失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERaddAccount(const std::string& phone, const std::string& password, const std::string& address) {
    if (!dbManager) {
        Logger::instance().fail("Server SERaddAccount: dbManager is null");
        nlohmann::json e;
        e["error"] = "服务器内部错误";
        return e.dump();
    }

    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) {
        Logger::instance().fail("Server SERaddAccount: 数据库未连接");
        nlohmann::json e;
        e["error"] = "数据库未连接";
        return e.dump();
    }

    try {
        User u(phone, password, address);  // 使用传入的address参数
        if (!dbManager->DTBaddUser(u)) {
            nlohmann::json r;
            r["error"] = "添加失败";
            return r.dump();
        }

        nlohmann::json ok;
        ok["result"] = "added";
        ok["phone"] = phone;
        ok["address"] = address;  // 可选：返回地址信息
        return ok.dump();

    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERaddAccount 异常: ") + e.what());
        nlohmann::json err;
        err["error"] = "添加失败";
        err["message"] = e.what();
        return err.dump();
    }
}

std::string Server::SERupdateUser(const std::string & phone, const std::string & password, const std::string & address) {
    if (!dbManager) { Logger::instance().fail("Server SERupdateUser: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
     if (!dbManager->DTBisConnected()) {
        Logger::instance().warn("Server SERupdateUser: dbManager 未连接，尝试重新连接...");
        if (!dbManager->DTBinitialize()) {
            Logger::instance().fail("Server SERupdateUser: 重新连接数据库失败");
            nlohmann::json e; e["error"] = "数据库未连接"; return e.dump();
            
        }
        
    }
     try {
        User u(phone, password, address);
        if (!dbManager->DTBupdateUser(u)) {
            nlohmann::json r; r["error"] = "更新失败"; return r.dump();
            
        }
         nlohmann::json ok; ok["result"] = "updated"; ok["phone"] = phone;
        return ok.dump();
        
    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERupdateUser 异常: ") + e.what());
        nlohmann::json err; err["error"] = "更新失败"; err["message"] = e.what(); return err.dump();
        
    }
    catch (...) {
        Logger::instance().fail("Server SERupdateUser 发生未知异常。");
        nlohmann::json err; err["error"] = "更新失败"; err["message"] = "未知异常"; return err.dump();
        
    }
    
}

// ---------- Order APIs (basic) ----------
std::string Server::SERgetAllOrders(const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERgetAllOrders: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERgetAllOrders: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        auto orders = dbManager->DTBloadOrdersByUser(userPhone);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& o : orders) {
            nlohmann::json jo;
            jo["order_id"] = o.getOrderId();
            jo["status"] = o.getStatus();
            jo["final_amount"] = o.getFinalAmount();
            arr.push_back(jo);
        }
        return arr.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERgetAllOrders 异常: ") + e.what()); nlohmann::json err; err["error"] = "查询失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERgetOrderDetail(const std::string& orderId, const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERgetOrderDetail: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERgetOrderDetail: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Order o;
        if (!dbManager->DTBloadOrder(orderId, o)) { nlohmann::json r; r["error"] = "未找到订单"; return r.dump(); }
        nlohmann::json jo;
        jo["order_id"] = o.getOrderId();
        jo["status"] = o.getStatus();
        jo["final_amount"] = o.getFinalAmount();
        // items
        nlohmann::json items = nlohmann::json::array();
        for (const auto& it : o.getItems()) {
            items.push_back({{"good_id", it.getGoodId()}, {"good_name", it.getGoodName()}, {"price", it.getPrice()}, {"quantity", it.getQuantity()}, {"subtotal", it.getSubtotal()}});
        }
        jo["items"] = items;
        return jo.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERgetOrderDetail 异常: ") + e.what()); nlohmann::json err; err["error"] = "查询失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERupdateOrderStatus(const std::string& orderId, const std::string& userPhone, int newStatus) {
    if (!dbManager) { Logger::instance().fail("Server SERupdateOrderStatus: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERupdateOrderStatus: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        if (!dbManager->DTBupdateOrderStatus(orderId, newStatus)) { nlohmann::json r; r["error"] = "更新失败"; return r.dump(); }
        nlohmann::json ok; ok["result"] = "updated"; ok["order_id"] = orderId; ok["status"] = newStatus; return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERupdateOrderStatus 异常: ") + e.what()); nlohmann::json err; err["error"] = "更新失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERaddSettledOrder(const std::string& orderId, const std::string& productName, int productId,
                                       int quantity, const std::string& userPhone, int status, const std::string& discountPolicy) {
    if (!dbManager) { Logger::instance().fail("Server SERaddSettledOrder: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERaddSettledOrder: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Order o(TemporaryCart(), orderId);
        o.setStatus(status);
        o.setShippingAddress("");
        o.setDiscountPolicy(discountPolicy);
        // add one item
        OrderItem it; it.setOrderId(orderId); it.setGoodId(productId); it.setGoodName(productName); it.setPrice(0.0); it.setQuantity(quantity); it.setSubtotal(0.0);
        o.setItems({it});
        if (!dbManager->DTBsaveOrder(o)) { nlohmann::json r; r["error"] = "保存失败"; return r.dump(); }
        nlohmann::json ok; ok["result"] = "added"; ok["order_id"] = orderId; return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERaddSettledOrder 异常: ") + e.what()); nlohmann::json err; err["error"] = "添加失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERreturnSettledOrder(const std::string& orderId, const std::string& userPhone) {
    // 简单代理到删除
    return SERdeleteSettledOrder(orderId, userPhone);
}

std::string Server::SERrepairSettledOrder(const std::string& orderId, const std::string& userPhone) {
    // 未实现复杂修复逻辑，返回未实现
    nlohmann::json res; res["error"] = "not_implemented"; res["message"] = "repair not implemented"; return res.dump();
}

std::string Server::SERdeleteSettledOrder(const std::string& orderId, const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERdeleteSettledOrder: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERdeleteSettledOrder: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        if (!dbManager->DTBdeleteOrder(orderId)) { nlohmann::json r; r["error"] = "删除失败"; return r.dump(); }
        nlohmann::json ok; ok["result"] = "deleted"; ok["order_id"] = orderId; return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERdeleteSettledOrder 异常: ") + e.what()); nlohmann::json err; err["error"] = "删除失败"; err["message"] = e.what(); return err.dump(); }
}

// ---------- Cart APIs ----------
static void recalcCartTotals(TemporaryCart& cart) {
    double total = 0.0;
    for (auto& it : cart.items) total += it.subtotal;
    cart.total_amount = total;
    cart.final_amount = total; // no discount logic for now
}

std::string Server::SERgetCart(const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERgetCart: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERgetCart: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        TemporaryCart cart;
        // 使用按 user_phone 查找的 DB 方法（可同时加载 items）
        if (!dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart)) {
            nlohmann::json r; r["error"] = "未找到购物车"; return r.dump();
        }
        nlohmann::json j;
        j["cart_id"] = cart.cart_id;
        j["user_phone"] = cart.user_phone;
        j["shipping_address"] = cart.shipping_address;
        j["discount_policy"] = cart.discount_policy;
        j["total_amount"] = cart.total_amount;
        j["discount_amount"] = cart.discount_amount;
        j["final_amount"] = cart.final_amount;
        j["is_converted"] = cart.is_converted;
        nlohmann::json items = nlohmann::json::array();
        for (const auto& it : cart.items) {
            items.push_back({{"good_id", it.good_id}, {"good_name", it.good_name}, {"price", it.price}, {"quantity", it.quantity}, {"subtotal", it.subtotal}});
        }
        j["items"] = items;
        return j.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERgetCart 异常: ") + e.what()); nlohmann::json err; err["error"] = "查询失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERsaveCart(const std::string& userPhone, const std::string& cartData) {
    if (!dbManager) { Logger::instance().fail("Server SERsaveCart: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERsaveCart: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        auto j = nlohmann::json::parse(cartData);
        TemporaryCart cart;

        // 如果客户端传入 cart_id 使用之，否则生成基于时间的随机 cart_id
        if (j.contains("cart_id") && j["cart_id"].is_string() && !j["cart_id"].get<std::string>().empty()) {
            cart.cart_id = j["cart_id"].get<std::string>();
        } else {
            cart.cart_id = generateCartId(userPhone);
        }

        cart.user_phone = userPhone;
        cart.shipping_address = j.value("shipping_address", std::string(""));
        cart.discount_policy = j.value("discount_policy", std::string(""));
        cart.is_converted = j.value("is_converted", false);
        cart.items.clear();
        if (j.contains("items") && j["items"].is_array()) {
            for (const auto& it : j["items"]) {
                CartItem ci; ci.good_id = it.value("good_id", 0);
                ci.good_name = it.value("good_name", std::string(""));
                ci.price = it.value("price", 0.0);
                ci.quantity = it.value("quantity", 0);
                ci.subtotal = it.value("subtotal", ci.price * ci.quantity);
                cart.items.push_back(ci);
            }
        }
        recalcCartTotals(cart);
        if (!dbManager->DTBsaveTemporaryCart(cart)) {
            // try update
            if (!dbManager->DTBupdateTemporaryCart(cart)) {
                nlohmann::json r; r["error"] = "保存失败"; return r.dump();
            }
        }
        nlohmann::json ok; ok["result"] = "saved"; ok["cart_id"] = cart.cart_id; return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERsaveCart 异常: ") + e.what()); nlohmann::json err; err["error"] = "保存失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERaddToCart(const std::string& userPhone, int productId, const std::string& productName, double price, int quantity) {
    if (!dbManager) { Logger::instance().fail("Server SERaddToCart: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERaddToCart: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        TemporaryCart cart;
        // 先按 user_phone 查找是否存在临时购物车（避免为同一用户创建多条 cart）
        bool exists = dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart);
        if (!exists) {
            cart.cart_id = generateCartId(userPhone);
            cart.user_phone = userPhone;
            cart.items.clear();
            cart.total_amount = cart.final_amount = 0.0;
            cart.is_converted = false;
        }
        bool found = false;
        for (auto& it : cart.items) {
            if (it.good_id == productId) { it.quantity += quantity; it.subtotal = it.price * it.quantity; found = true; break; }
        }
        if (!found) {
            CartItem ci; ci.good_id = productId; ci.good_name = productName; ci.price = price; ci.quantity = quantity; ci.subtotal = price * quantity; cart.items.push_back(ci);
        }
        recalcCartTotals(cart);
        if (exists) {
            if (!dbManager->DTBupdateTemporaryCart(cart)) { nlohmann::json r; r["error"] = "更新购物车失败"; return r.dump(); }
        } else {
            if (!dbManager->DTBsaveTemporaryCart(cart)) { nlohmann::json r; r["error"] = "保存购物车失败"; return r.dump(); }
        }
        nlohmann::json ok; ok["result"] = "ok"; ok["cart_id"] = cart.cart_id; return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERaddToCart 异常: ") + e.what()); nlohmann::json err; err["error"] = "添加失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERupdateCartItem(const std::string& userPhone, int productId, int quantity) {
    if (!dbManager) { Logger::instance().fail("Server SERupdateCartItem: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERupdateCartItem: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Logger::instance().info(std::string("Server SERupdateCartItem: request userPhone=") + userPhone + ", productId=" + std::to_string(productId) + ", quantity=" + std::to_string(quantity));
        TemporaryCart cart;
        // 修正：按 userPhone 查找最新临时购物车（原来错误地把 userPhone 当作 cart_id 调用 DTBloadTemporaryCart）
        if (!dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart)) { nlohmann::json r; r["error"] = "未找到购物车"; Logger::instance().warn("Server SERupdateCartItem: 未找到购物车 for userPhone=" + userPhone); return r.dump(); }
        bool found = false;
        for (auto& it : cart.items) {
            if (it.good_id == productId) { it.quantity = quantity; it.subtotal = it.price * it.quantity; found = true; break; }
        }
        if (!found) { nlohmann::json r; r["error"] = "未找到商品"; Logger::instance().warn("Server SERupdateCartItem: 未找到商品 productId=" + std::to_string(productId) + " in cart"); return r.dump(); }
        recalcCartTotals(cart);
        if (!dbManager->DTBupdateTemporaryCart(cart)) { nlohmann::json r; r["error"] = "更新失败"; Logger::instance().fail("Server SERupdateCartItem: DTBupdateTemporaryCart 返回 false"); return r.dump(); }
        nlohmann::json ok; ok["result"] = "updated"; ok["cart_id"] = cart.cart_id;
        Logger::instance().info(std::string("Server SERupdateCartItem: updated productId=") + std::to_string(productId) + " qty=" + std::to_string(quantity) + " cart_id=" + cart.cart_id);
        return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERupdateCartItem 异常: ") + e.what()); nlohmann::json err; err["error"] = "更新失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERremoveFromCart(const std::string& userPhone, int productId) {
    if (!dbManager) { Logger::instance().fail("Server SERremoveFromCart: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERremoveFromCart: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Logger::instance().info(std::string("Server SERremoveFromCart: request userPhone=") + userPhone + ", productId=" + std::to_string(productId));
        TemporaryCart cart;
        // 修正：按 userPhone 查找最新临时购物车
        if (!dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart)) { nlohmann::json r; r["error"] = "未找到购物车"; Logger::instance().warn("Server SERremoveFromCart: 未找到购物车 for userPhone=" + userPhone); return r.dump(); }
        auto it = std::remove_if(cart.items.begin(), cart.items.end(), [productId](const CartItem& ci){ return ci.good_id == productId; });
        if (it == cart.items.end()) { nlohmann::json r; r["error"] = "未找到商品"; Logger::instance().warn("Server SERremoveFromCart: 未找到商品 productId=" + std::to_string(productId) + " in cart"); return r.dump(); }
        cart.items.erase(it, cart.items.end());
        recalcCartTotals(cart);
        if (!dbManager->DTBupdateTemporaryCart(cart)) { nlohmann::json r; r["error"] = "更新失败"; Logger::instance().fail("Server SERremoveFromCart: DTBupdateTemporaryCart 返回 false"); return r.dump(); }
        nlohmann::json ok; ok["result"] = "removed"; ok["cart_id"] = cart.cart_id;
        Logger::instance().info(std::string("Server SERremoveFromCart: removed productId=") + std::to_string(productId) + " cart_id=" + cart.cart_id);
        return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERremoveFromCart 异常: ") + e.what()); nlohmann::json err; err["error"] = "删除失败"; err["message"] = e.what(); return err.dump(); }
}

// promotion APIs are intentionally left unimplemented for now
std::string Server::SERgetAllPromotions() { nlohmann::json r; r["error"] = "not_implemented"; return r.dump(); }
std::string Server::SERgetPromotionsByProductId(int productId) { nlohmann::json r; r["error"] = "not_implemented"; return r.dump(); }
std::string Server::SERupdateCartForPromotions(const std::string& userPhone) { nlohmann::json r; r["error"] = "not_implemented"; return r.dump(); }

