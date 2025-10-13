#include "Server.h"
#include "logger.h"
#include <QHostAddress>
#include <nlohmann/json.hpp>
#include <qdatetime.h>
#include <chrono>
#include <random>
#include <sstream>
#include <fstream> // 为了 std::ofstream
// --- 增强日志与失败请求记录（添加到 Server.cpp，放在顶部辅助函数区）---
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <cerrno>

static void ensureDirExists(const std::string& dir) {
#ifdef _WIN32
    int res = _mkdir(dir.c_str());
    if (res != 0 && errno != EEXIST) {
        Logger::instance().warn(std::string("ensureDirExists: _mkdir failed for ") + dir + " errno=" + std::to_string(errno));
    }
#else
    if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
        Logger::instance().warn(std::string("ensureDirExists: mkdir failed for ") + dir + " errno=" + std::to_string(errno));
    }
#endif
}

static double computeFinalFromPolicy(const nlohmann::json& policy, const nlohmann::json& items, double originalTotal);
static std::string derivePolicyName(const nlohmann::json& policy);

static void recalcCartTotalsImpl_local(TemporaryCart& cart, const nlohmann::json& policy, DatabaseManager* dbManager) {
    // 计算原总额
    double original_total = 0.0;
    nlohmann::json itemsJson = nlohmann::json::array();
    for (const auto& it : cart.items) {
        double subtotal = it.price * static_cast<double>(it.quantity);
        original_total += subtotal;
        itemsJson.push_back({
            {"good_id", it.good_id},
            {"good_name", it.good_name},
            {"price", it.price},
            {"quantity", it.quantity},
            {"subtotal", subtotal}
            });
    }

    // 优先使用传入的 policy（若为 object）
    double new_total = original_total;
    if (policy.is_object()) {
        try {
            new_total = computeFinalFromPolicy(policy, itemsJson, original_total);
            // 如果 discount_policy 为空，则派生一个名称写入（便于展示）
            if (cart.discount_policy.empty()) {
                std::string name = derivePolicyName(policy);
                if (!name.empty()) cart.discount_policy = name;
            }
        }
        catch (...) {
            // 解析/应用 policy 失败 -> 回退为原价
            new_total = original_total;
        }
    }
    else {
        // 否则使用服务端现有的促销引擎（按单品查询最佳策略）
        new_total = 0.0;

        // 如果没有 dbManager，则无法查询数据库促销，直接按原价
        std::vector<std::map<std::string, std::string>> rows;
        if (dbManager) {
            try {
                rows = dbManager->DTBloadAllPromotionStrategies(false);
            }
            catch (...) {
                rows.clear();
            }
        }

        for (auto& it : cart.items) {
            double bestSubtotal = it.price * static_cast<double>(it.quantity);

            // 如果有 db 数据，则解析并尝试应用适配该商品的策略
            if (!rows.empty()) {
                for (const auto& m : rows) {
                    try {
                        if (!m.count("policy_detail")) continue;
                        json p = json::parse(m.at("policy_detail"));
                        bool applies = false;
                        // 全局适用
                        if (p.contains("scope") && p["scope"].is_string() && p["scope"] == "global") applies = true;
                        // 按 product_ids 匹配
                        if (p.contains("product_ids") && p["product_ids"].is_array()) {
                            for (const auto& pid : p["product_ids"]) {
                                if (pid.is_number() && pid.get<int>() == it.good_id) { applies = true; break; }
                                if (pid.is_string()) {
                                    try { if (std::stoi(pid.get<std::string>()) == it.good_id) { applies = true; break; } }
                                    catch (...) {}
                                }
                            }
                        }
                        if (!applies) continue;

                        // 应用策略（使用 PromotionStrategy 辅助）
                        json pol = p;
                        auto strat = PromotionStrategy::fromJson(pol);
                        double applied = strat->apply(it.price, it.quantity);
                        if (applied < bestSubtotal) bestSubtotal = applied;
                    }
                    catch (...) {
                        // 单条策略解析/应用失败则跳过
                        continue;
                    }
                }
            }

            // 如果没有 dbManager 或没有匹配策略，则 bestSubtotal 保持原值
            new_total += bestSubtotal;
        }
    }

    cart.total_amount = original_total;
    cart.final_amount = new_total;
    cart.discount_amount = std::max(0.0, original_total - new_total);
}

static std::string sanitizeFileName(const std::string& s) {
    std::string out = s;
    for (auto &c : out) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
    }
    return out;
}

static nlohmann::json orderToJson(const Order& o) {
    nlohmann::json jo;
    jo["order_id"] = o.getOrderId();
    jo["user_phone"] = o.getUserPhone();
    jo["shipping_address"] = o.getShippingAddress();
    jo["status"] = o.getStatus();
    jo["discount_policy"] = o.getDiscountPolicy();
    jo["total_amount"] = o.getTotalAmount();
    jo["discount_amount"] = o.getDiscountAmount();
    jo["final_amount"] = o.getFinalAmount();
    jo["items"] = nlohmann::json::array();
    for (const auto& it : o.getItems()) {
        nlohmann::json ji;
        ji["good_id"] = it.getGoodId();
        ji["good_name"] = it.getGoodName();
        ji["price"] = it.getPrice();
        ji["quantity"] = it.getQuantity();
        ji["subtotal"] = it.getSubtotal();
        jo["items"].push_back(ji);
    }
    return jo;
}

static bool saveOrderWithLogging(DatabaseManager* db, const Order& o, const nlohmann::json& rawRequestJson = nlohmann::json()) {
    Logger::instance().info(std::string("Server: attempt DTBsaveOrder order_id=") + o.getOrderId() + " user=" + o.getUserPhone());
    bool ok = db->DTBsaveOrder(o);
    if (!ok) {
        Logger::instance().fail(std::string("Server: DTBsaveOrder FAILED for order_id=") + o.getOrderId());
        try {
            ensureDirExists("failed_orders");
            auto jo = orderToJson(o);
            nlohmann::json out;
            out["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
            out["order"] = jo;
            if (!rawRequestJson.is_null()) out["raw_request"] = rawRequestJson;
            std::string fname = std::string("failed_orders/order_") + sanitizeFileName(o.getOrderId()) + "_" + std::to_string(QDateTime::currentMSecsSinceEpoch()) + ".json";
            std::ofstream ofs(fname, std::ios::out | std::ios::trunc);
            if (ofs.is_open()) {
                ofs << out.dump(4);
                ofs.close();
                Logger::instance().info(std::string("Server: dumped failed order to ") + fname);
            } else {
                Logger::instance().fail(std::string("Server: unable to open file to write failed order: ") + fname);
            }
        } catch (const std::exception& ex) {
            Logger::instance().fail(std::string("Server: exception while writing failed order: ") + ex.what());
        }
    } else {
        Logger::instance().info(std::string("Server: DTBsaveOrder succeeded order_id=") + o.getOrderId());
    }
    return ok;
}

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
    // 格式: cYYYYMMDDHHMMSSmmm_<hex-rand>
    // 使用 QDateTime 获取可读的年月日时分秒与毫秒，保留随机后缀以避免冲突
    QString dt = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz"); // 包含毫秒
    // 线程局部随机数引擎
    static thread_local std::mt19937_64 rng((std::random_device())());
    std::uniform_int_distribution<uint64_t> dist(0, std::numeric_limits<uint64_t>::max());
    uint64_t r = dist(rng);

    std::ostringstream oss;
    oss << "c" << dt.toStdString() << "_" << std::hex << r;
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

                    // 记录到 Logger：每接收到一次请求就记录一条信息
                    try {
                        std::string recv = reqStr.toStdString();
                        // 截断过长日志，避免 log 被淹没
                        const size_t MaxLogLen = 1024;
                        if (recv.size() > MaxLogLen) recv = recv.substr(0, MaxLogLen) + "...(truncated)";
                        Logger::instance().info(std::string("Server received request: ") + recv);
                    } catch (...) {
                        // 记录异常不影响主流程
                        Logger::instance().warn("Server: failed to log received request (exception)");
                    }

                    std::string response = SERprocessRequest(reqStr.toStdString());

                    // 记录将要发送的响应（摘要）
                    try {
                        std::string respSummary = response;
                        const size_t MaxRespLog = 1024;
                        if (respSummary.size() > MaxRespLog) respSummary = respSummary.substr(0, MaxRespLog) + "...(truncated)";
                        Logger::instance().info(std::string("Server sending response (len=") + std::to_string(response.size()) + "): " + respSummary);
                    } catch (...) {
                        Logger::instance().warn("Server: failed to log response (exception)");
                    }

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

        // 统一记录已解析的命令与有效负载摘要
        try {
            std::string payload = rest;
            const size_t MaxPayloadLog = 512;
            if (payload.size() > MaxPayloadLog) payload = payload.substr(0, MaxPayloadLog) + "...(truncated)";
            Logger::instance().info(std::string("Server::SERprocessRequest: cmd=") + (cmd.empty() ? "<empty>" : cmd)
                                    + " payload=" + (payload.empty() ? "<none>" : payload));
        } catch (...) {
            Logger::instance().warn("Server::SERprocessRequest: failed to log command/payload");
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
            // 在 ADD_SETTLED_ORDER 处理分支的 try 块顶部添加此行，定义 stockChecks 变量
            struct StockCheck {
                int productId;
                int requested;
                int available;
            };
            std::vector<StockCheck> stockChecks;
            try {
                auto j = parseJson(rest);
                // 如果客户端发来完整 items 数组，按完整订单处理
                if (j.is_object() && j.contains("items") && j["items"].is_array()) {
                    TemporaryCart cart;
                    // 接受多种命名风格：优先 camelCase, 回退 snake_case
                    cart.cart_id = j.value("orderId", j.value("order_id", std::string("")));
                    cart.user_phone = j.value("userPhone", j.value("user_phone", std::string("")));
                    cart.shipping_address = j.value("shippingAddress", j.value("shipping_address", std::string("")));
                    cart.discount_policy = j.value("discountPolicy", std::string(""));
                    cart.items.clear();
                    double total = 0.0;
                    for (const auto& it : j["items"]) {
                        CartItem ci;
                        ci.good_id = it.value("productId", 0);
                        ci.good_name = it.value("productName", std::string(""));
                        ci.price = it.value("price", 0.0);
                        ci.quantity = it.value("quantity", 0);
                        ci.subtotal = it.value("subtotal", ci.price * ci.quantity);
                        if (ci.subtotal <= 0.0) ci.subtotal = ci.price * ci.quantity;
                        total += ci.subtotal;
                        cart.items.push_back(ci);

                        // 检查库存可用性（延迟到保存前统一检查）
                        Good gtmp;
                        int avail = -1;
                        if (dbManager && dbManager->DTBisConnected() && dbManager->DTBloadGood(ci.good_id, gtmp)) {
                            avail = gtmp.getStock();
                        }
                        stockChecks.push_back({ci.good_id, ci.quantity, avail});
                    }
                    // 进行库存可用性校验
                    for (const auto& sc : stockChecks) {
                        if (sc.available < 0) {
                            nlohmann::json r; r["error"] = "good_not_found"; r["productId"] = sc.productId; return r.dump();
                        }
                        if ((long long)sc.requested > (long long)sc.available) {
                            nlohmann::json r; r["error"] = "stock_exceeded"; r["productId"] = sc.productId; r["available"] = sc.available; r["requested"] = sc.requested; return r.dump();
                        }
                    }

                    cart.total_amount = total;
                    cart.final_amount = total;
                    cart.discount_amount = 0.0;
                    cart.is_converted = false;

                    // 尝试提取客户端随请求传来的 policy（兼容多种字段名）
                    nlohmann::json providedPolicy;
                    if (j.contains("policy")) providedPolicy = j["policy"];
                    else if (j.contains("policy_detail")) {
                        try { providedPolicy = nlohmann::json::parse(j.value("policy_detail", std::string(""))); } catch (...) { providedPolicy = nlohmann::json(); }
                    } else if (j.contains("policy_str")) {
                        try { providedPolicy = nlohmann::json::parse(j.value("policy_str", std::string(""))); } catch (...) { providedPolicy = nlohmann::json(); }
                    } else {
                        providedPolicy = nlohmann::json();
                    }

                    // 优先：如果客户端提供了 final_amount/discount_amount 字段，则尊重（但仍可校验）；
                    // 次优：如果服务器上已经为该用户保存了 TemporaryCart（相同 cart_id），使用服务器保存的 final_amount（更可信）
                    // 否则：根据 providedPolicy 或服务端促销引擎计算 final_amount
                    bool usedServerSavedCart = false;
                    if (j.contains("final_amount") && j.contains("discount_amount")) {
                        // 客户端显式发送了金额（通常来自客户端本地计算或从服务器读取后传回）
                        cart.final_amount = j.value("final_amount", cart.total_amount);
                        cart.discount_amount = j.value("discount_amount", 0.0);
                        if (cart.discount_policy.empty()) cart.discount_policy = j.value("discountPolicy", cart.discount_policy);
                    } else {
                        // 尝试加载服务器端已保存的购物车（按用户手机号），若存在且 cart_id 匹配并含 final_amount，则使用之（更可信）
                        TemporaryCart savedCart;
                        if (dbManager && dbManager->DTBisConnected() && dbManager->DTBloadTemporaryCartByUserPhone(cart.user_phone, savedCart)) {
                            if (!savedCart.cart_id.empty() && savedCart.cart_id == cart.cart_id && savedCart.final_amount > 0.0) {
                                cart.final_amount = savedCart.final_amount;
                                cart.discount_amount = savedCart.discount_amount;
                                if (cart.discount_policy.empty()) cart.discount_policy = savedCart.discount_policy;
                                usedServerSavedCart = true;
                            }
                        }
                        if (!usedServerSavedCart) {
                            // 没有服务器保存的结果，按优先级：providedPolicy（若为 object）-> 服务端促销引擎进行逐项匹配
                            Server::recalcCartTotalsImpl(cart, providedPolicy, dbManager);
                        }
                    }

                    // 构造 Order 并填充 items
                    Order o(cart, cart.cart_id, j.value("status", 1));

                    // 显式补全 userPhone / shippingAddress，避免 Order 构造/DB 保存遗漏
                    if (!cart.user_phone.empty()) o.setUserPhone(cart.user_phone);
                    if (!cart.shipping_address.empty()) o.setShippingAddress(cart.shipping_address);

                    o.setDiscountPolicy(cart.discount_policy);
                    o.setTotalAmount(cart.total_amount);
                    o.setDiscountAmount(cart.discount_amount);
                    o.setFinalAmount(cart.final_amount);

                    std::vector<OrderItem> orderItems;
                    for (const auto& ci : cart.items) {
                        OrderItem oi;
                        oi.setOrderId(cart.cart_id);
                        oi.setGoodId(ci.good_id);
                        oi.setGoodName(ci.good_name);
                        oi.setPrice(ci.price);
                        oi.setQuantity(ci.quantity);
                        oi.setSubtotal(ci.subtotal);
                        orderItems.push_back(oi);
                    }
                    o.setItems(orderItems);

                    // 保存订单前再次确保数据库连接可用
                    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) {
                        nlohmann::json r; r["error"] = "database_unavailable"; return r.dump();
                    }

                    // 保存订单
                    if (!dbManager->DTBsaveOrder(o)) {
                        nlohmann::json r; r["error"] = "保存失败"; return r.dump();
                    }

                    // 保存成功后，更新每个商品库存；若任一更新失败，尝试回滚已保存的订单
                    bool stockUpdateFailed = false;
                    for (const auto& it : o.getItems()) {
                        Good g;
                        if (!dbManager->DTBloadGood(it.getGoodId(), g)) {
                            Logger::instance().warn("Server SERaddSettledOrder: 无法加载商品以更新库存 good_id=" + std::to_string(it.getGoodId()));
                            stockUpdateFailed = true;
                            break;
                        }
                        int newStock = g.getStock() - it.getQuantity();
                        if (newStock < 0) newStock = 0;
                        if (!dbManager->DTBupdateGoodStock(it.getGoodId(), newStock)) {
                            Logger::instance().warn("Server SERaddSettledOrder: 更新库存失败 good_id=" + std::to_string(it.getGoodId()));
                            stockUpdateFailed = true;
                            break;
                        } else {
                            Logger::instance().info("Server SERaddSettledOrder: 减库存 good_id=" + std::to_string(it.getGoodId()) + " -> " + std::to_string(newStock));
                        }
                    }

                    if (stockUpdateFailed) {
                        // 尝试回滚：删除已保存的订单
                        if (!dbManager->DTBdeleteOrder(o.getOrderId())) {
                            Logger::instance().fail("Server SERaddSettledOrder: 库存更新失败，且订单回滚失败 order_id=" + o.getOrderId());
                            nlohmann::json r; r["error"] = "stock_update_and_rollback_failed"; r["order_id"] = o.getOrderId(); return r.dump();
                        }
                        Logger::instance().info("Server SERaddSettledOrder: 库存更新失败，已回滚订单 order_id=" + o.getOrderId());
                        nlohmann::json r; r["error"] = "stock_update_failed"; r["message"] = "库存更新失败，订单已回滚"; r["order_id"] = o.getOrderId(); return r.dump();
                    }

                    nlohmann::json ok; ok["result"] = "added"; ok["order_id"] = o.getOrderId();
                    return ok.dump();
                }
                // 否则兼容旧格式（单个商品字段）
                else {
                    auto j2 = parseJson(rest);
                    return SERaddSettledOrder(
                        j2.value("orderId", std::string("")),
                        j2.value("productName", std::string("")),
                        j2.value("productId", 0),
                        j2.value("quantity", 0),
                        j2.value("userPhone", std::string("")),
                        j2.value("status", 0),
                        j2.value("discountPolicy", std::string("")));
                }
            } catch (const std::exception& e) {
                Logger::instance().fail(std::string("Server ADD_SETTLED_ORDER 异常: ") + e.what());
                nlohmann::json err; err["error"] = "添加失败"; err["message"] = e.what(); return err.dump();
            } catch (...) {
                nlohmann::json err; err["error"] = "添加失败"; err["message"] = "未知异常"; return err.dump();
            }
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
        if (cmd == "ADD_TO_CART") {
            try {
                auto j = parseJson(rest);
                if (!j.is_object()) { nlohmann::json e; e["error"] = "invalid_payload"; return e.dump(); }
                // 兼容客户端发送的字段名：userPhone/productId/productName/price/quantity
                std::string userPhone = j.value("userPhone", std::string(""));
                int productId = j.value("productId", 0);
                std::string productName = j.value("productName", std::string(""));
                double price = j.value("price", 0.0);
                int quantity = j.value("quantity", 0);
                // 调用已经实现的服务器端处理函数
                return SERaddToCart(userPhone, productId, productName, price, quantity);
            } catch (...) {
                nlohmann::json e; e["error"] = "参数解析失败"; return e.dump();
            }
        }
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
        if (cmd == "SAVE_CART_WITH_POLICY") {
            try {
                auto j = parseJson(rest);
                if (!j.is_object()) {
                    nlohmann::json e; e["error"] = "invalid_payload"; return e.dump();
                }
                std::string userPhone = j.value("userPhone", std::string());
                if (userPhone.empty()) {
                    nlohmann::json e; e["error"] = "missing_userPhone"; return e.dump();
                }
                if (!j.contains("cart") || !j["cart"].is_object()) {
                    nlohmann::json e; e["error"] = "missing_cart"; return e.dump();
                }

                nlohmann::json cartObj = j["cart"];

                // 如果客户端同时传来 policyJson（字符串），解析并放入 cartObj["policy"]
                std::string policyJsonStr = j.value("policyJson", std::string());
                if (!policyJsonStr.empty()) {
                    try {
                        cartObj["policy"] = nlohmann::json::parse(policyJsonStr);
                    } catch (...) {
                        // 若解析失败，则保留为字符串形式（SERsaveCart 内部也会容错）
                        cartObj["policy"] = policyJsonStr;
                    }
                }

                Logger::instance().info("Server: handling SAVE_CART_WITH_POLICY for userPhone=" + userPhone);
                // 复用现有保存逻辑（SERsaveCart 会从 cart JSON 中提取 policy 并调用 recalc）
                return SERsaveCart(userPhone, cartObj.dump());
            } catch (const std::exception& ex) {
                Logger::instance().fail(std::string("Server SAVE_CART_WITH_POLICY 异常: ") + ex.what());
                nlohmann::json err; err["error"] = "exception"; err["message"] = ex.what(); return err.dump();
            } catch (...) {
                nlohmann::json err; err["error"] = "unknown_exception"; return err.dump();
            }
        }

        // promotion - not implemented but allow calls
        if (cmd == "GET_ALL_PROMOTIONS") return SERgetAllPromotions();
        if (cmd == "GET_PROMOTIONS_BY_PRODUCT_ID") {
            try { int pid = std::stoi(rest); return SERgetPromotionsByProductId(pid); }
            catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "UPDATE_CART_FOR_PROMOTIONS") return SERupdateCartForPromotions(rest);
        // Promotion admin operations (管理员增删改查)
        if (cmd == "ADD_PROMOTION") {
            // rest: json { "name": "...", "policy": { ... } , "type": "...", "conditions": "..." }
            try {
                auto j = parseJson(rest);
                if (!j.is_object()) { nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump(); }
                return SERaddPromotion(j);
            } catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "UPDATE_PROMOTION") {
            try {
                auto j = parseJson(rest);
                if (!j.is_object()) { nlohmann::json e; e["error"] = "参数缺失或格式错误"; return e.dump(); }
                return SERupdatePromotion(j);
            } catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }
        if (cmd == "DELETE_PROMOTION") {
            try {
                auto j = parseJson(rest);
                std::string name = j.is_object() ? j.value("name", std::string("")) : std::string();
                if (name.empty()) { nlohmann::json e; e["error"] = "missing_name"; return e.dump(); }
                return SERdeletePromotion(name);
            } catch (...) { nlohmann::json e; e["error"] = "参数解析失败"; return e.dump(); }
        }

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

// ---------- 订单相关 ----------
std::string Server::SERgetAllOrders(const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERgetAllOrders: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERgetAllOrders: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        std::vector<Order> orders;
        if (userPhone.empty()) {
            // 管理员/全局调用：当 userPhone 为空时，返回最近的订单（可改为 DTBloadAllOrders 若实现）
            orders = dbManager->DTBloadRecentOrders(200);
        }
        else {
            orders = dbManager->DTBloadOrdersByUser(userPhone);
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& o : orders) {
            nlohmann::json jo;
            jo["order_id"] = o.getOrderId();
            jo["user_phone"] = o.getUserPhone();
            // 新增：返回收货地址，兼容客户端读取 shipping_address 或 shippingAddress
            jo["shipping_address"] = o.getShippingAddress();
            jo["status"] = o.getStatus();
            // 返回金额信息以便客户端显示或做回退判断
            jo["total_amount"] = o.getTotalAmount();
            jo["discount_amount"] = o.getDiscountAmount();
            jo["final_amount"] = o.getFinalAmount();
            arr.push_back(jo);
        }
        return arr.dump();
    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERgetAllOrders 异常: ") + e.what());
        nlohmann::json err; err["error"] = "查询失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERgetOrderDetail(const std::string& orderId, const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail(std::string("Server SERgetOrderDetail: dbManager is null")); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail(std::string("Server SERgetOrderDetail: 数据库未连接")); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Order o;
        if (!dbManager->DTBloadOrder(orderId, o)) { nlohmann::json r; r["error"] = "未找到订单"; return r.dump(); }

        // 可选：校验请求手机号与订单手机号是否匹配（如果需要权限控制）
        if (!userPhone.empty() && o.getUserPhone() != userPhone) {
            // 若不希望暴露，则返回 forbidden；否则可注释掉以下两行以允许管理员查看
            // nlohmann::json r; r["error"] = "forbidden"; r["message"] = "手机号与订单不匹配"; return r.dump();
        }

        nlohmann::json jo;
        jo["order_id"] = o.getOrderId();
        jo["user_phone"] = o.getUserPhone();                 // 新增：返回手机号
        jo["shipping_address"] = o.getShippingAddress();     // 新增：返回收货地址
        jo["status"] = o.getStatus();
        jo["total_amount"] = o.getTotalAmount();            // 建议返回：总金额
        jo["discount_amount"] = o.getDiscountAmount();      // 建议返回：优惠金额
        jo["final_amount"] = o.getFinalAmount();
        jo["discount_policy"] = o.getDiscountPolicy();      // 建议返回：使用的优惠策略

        // items
        nlohmann::json items = nlohmann::json::array();
        for (const auto& it : o.getItems()) {
            items.push_back({
                {"good_id", it.getGoodId()},
                {"good_name", it.getGoodName()},
                {"price", it.getPrice()},
                {"quantity", it.getQuantity()},
                {"subtotal", it.getSubtotal()}
                });
        }
        jo["items"] = items;

        return jo.dump();
    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERgetOrderDetail 异常: ") + e.what());
        nlohmann::json err; err["error"] = "查询失败"; err["message"] = e.what(); return err.dump();
    }
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
        // 旧单项接口：仍要检查库存
        Good g;
        if (!dbManager->DTBloadGood(productId, g)) { nlohmann::json r; r["error"] = "good_not_found"; return r.dump(); }
        if (quantity > g.getStock()) { nlohmann::json r; r["error"] = "stock_exceeded"; r["available"] = g.getStock(); r["requested"] = quantity; return r.dump(); }

        Order o(TemporaryCart(), orderId);
        o.setStatus(status);
        // 补充：设置 userPhone（来自参数）
        if (!userPhone.empty()) o.setUserPhone(userPhone);
        o.setShippingAddress("");
        o.setDiscountPolicy(discountPolicy);
        OrderItem it; it.setOrderId(orderId); it.setGoodId(productId); it.setGoodName(productName); it.setPrice(g.getPrice()); it.setQuantity(quantity); it.setSubtotal(g.getPrice() * quantity);
        o.setItems({it});
        // 保存订单并在保存成功后更新库存
        if (!dbManager->DTBsaveOrder(o)) { nlohmann::json r; r["error"] = "保存失败"; return r.dump(); }
        // 更新库存（简化：直接用当前库存 - 数量）
        int newStock = g.getStock() - quantity;
        if (newStock < 0) newStock = 0;
        dbManager->DTBupdateGoodStock(productId, newStock);
        nlohmann::json ok; ok["result"] = "added"; ok["order_id"] = orderId;
        return ok.dump();
    } catch (const std::exception& e) { Logger::instance().fail(std::string("Server SERaddSettledOrder 异常: ") + e.what()); nlohmann::json err; err["error"] = "添加失败"; err["message"] = e.what(); return err.dump(); }
}

std::string Server::SERreturnSettledOrder(const std::string& orderId, const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERreturnSettledOrder: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERreturnSettledOrder: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Order o;
        if (!dbManager->DTBloadOrder(orderId, o)) {
            nlohmann::json r; r["error"] = "未找到订单"; return r.dump();
        }
        if (!userPhone.empty() && o.getUserPhone() != userPhone) {
            nlohmann::json r; r["error"] = "forbidden"; r["message"] = "手机号与订单不匹配"; return r.dump();
        }

        // 约定退货状态码（如需调整请与项目中的状态定义保持一致）
        const int RETURNED_STATUS = 3;

        // 更新订单状态为已退货
        if (!dbManager->DTBupdateOrderStatus(orderId, RETURNED_STATUS)) {
            nlohmann::json r; r["error"] = "更新失败"; r["message"] = "无法设置退货状态"; return r.dump();
        }

        // 退货时尝试恢复商品库存（若商品存在）
        for (const auto& it : o.getItems()) {
            Good g;
            if (dbManager->DTBloadGood(it.getGoodId(), g)) {
                int newStock = g.getStock() + it.getQuantity();
                // 防护：避免负数（理论上不需要，但保险）
                if (newStock < 0) newStock = 0;
                if (!dbManager->DTBupdateGoodStock(it.getGoodId(), newStock)) {
                    Logger::instance().warn("Server SERreturnSettledOrder: 无法更新库存 good_id=" + std::to_string(it.getGoodId()));
                } else {
                    Logger::instance().info("Server SERreturnSettledOrder: 恢复库存 good_id=" + std::to_string(it.getGoodId()) + " -> " + std::to_string(newStock));
                }
            } else {
                Logger::instance().warn("Server SERreturnSettledOrder: 未找到商品以恢复库存 good_id=" + std::to_string(it.getGoodId()));
            }
        }

        nlohmann::json ok; ok["result"] = "updated"; ok["order_id"] = orderId; ok["status"] = RETURNED_STATUS;
        Logger::instance().info("Server SERreturnSettledOrder: set status to RETURNED for order " + orderId);
        return ok.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERreturnSettledOrder 异常: ") + e.what());
        nlohmann::json err; err["error"] = "退货失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERrepairSettledOrder(const std::string& orderId, const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERrepairSettledOrder: dbManager is null"); nlohmann::json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERrepairSettledOrder: 数据库未连接"); nlohmann::json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Order o;
        if (!dbManager->DTBloadOrder(orderId, o)) {
            nlohmann::json r; r["error"] = "未找到订单"; return r.dump();
        }
        if (!userPhone.empty() && o.getUserPhone() != userPhone) {
            nlohmann::json r; r["error"] = "forbidden"; r["message"] = "手机号与订单不匹配"; return r.dump();
        }

        // 约定维修状态码（如需调整请与项目中的状态定义保持一致）
        const int REPAIR_STATUS = 4;

        if (!dbManager->DTBupdateOrderStatus(orderId, REPAIR_STATUS)) {
            nlohmann::json r; r["error"] = "更新失败"; r["message"] = "无法设置维修状态"; return r.dump();
        }

        nlohmann::json ok; ok["result"] = "updated"; ok["order_id"] = orderId; ok["status"] = REPAIR_STATUS;
        Logger::instance().info("Server SERrepairSettledOrder: set status to REPAIR for order " + orderId);
        return ok.dump();
    } catch (const std::exception& e) {
        Logger::instance().fail(std::string("Server SERrepairSettledOrder 异常: ") + e.what());
        nlohmann::json err; err["error"] = "维修失败"; err["message"] = e.what(); return err.dump();
    }
}

std::string Server::SERdeleteSettledOrder(const std::string& orderId, const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERdeleteSettledOrder: dbManager is null"); json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERdeleteSettledOrder: 数据库未连接"); json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        // 验证订单存在并（可选）检查手机号匹配
        Order o;
        if (!dbManager->DTBloadOrder(orderId, o)) {
            json r; r["error"] = "未找到订单"; return r.dump();
        }
        if (!userPhone.empty() && o.getUserPhone() != userPhone) {
            json r; r["error"] = "forbidden"; r["message"] = "手机号与订单不匹配"; return r.dump();
        }

        // 删除订单项与订单
        if (!dbManager->DTBdeleteOrder(orderId)) {
            json r; r["error"] = "删除失败"; return r.dump();
        }
        json ok; ok["result"] = "deleted"; ok["order_id"] = orderId;
        Logger::instance().info("Server SERdeleteSettledOrder: deleted order " + orderId);
        return ok.dump();
    }
    catch (const std::exception& ex) {
        Logger::instance().fail(std::string("Server SERdeleteSettledOrder 异常: ") + ex.what());
        json err; err["error"] = "删除失败"; err["message"] = ex.what(); return err.dump();
    }
}

std::string Server::SERgetCart(const std::string& userPhone) {
    if (!dbManager) { Logger::instance().fail("Server SERgetCart: dbManager is null"); json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERgetCart: 数据库未连接"); json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        TemporaryCart cart;
        if (!dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart)) {
            json r; r["error"] = "未找到购物车"; return r.dump();
        }
        json j;
        j["cart_id"] = cart.cart_id;
        j["user_phone"] = cart.user_phone;
        j["shipping_address"] = cart.shipping_address;
        j["discount_policy"] = cart.discount_policy;
        j["total_amount"] = cart.total_amount;
        j["discount_amount"] = cart.discount_amount;
        j["final_amount"] = cart.final_amount;
        j["is_converted"] = cart.is_converted;
        j["items"] = json::array();
        for (const auto& it : cart.items) {
            j["items"].push_back({
                {"good_id", it.good_id},
                {"good_name", it.good_name},
                {"price", it.price},
                {"quantity", it.quantity},
                {"subtotal", it.subtotal}
                });
        }
        return j.dump();
    }
    catch (const std::exception& ex) {
        Logger::instance().fail(std::string("Server SERgetCart 异常: ") + ex.what());
        json err; err["error"] = "查询失败"; err["message"] = ex.what(); return err.dump();
    }
}

// 替换整个 Server::SERsaveCart 函数，确保如果客户端随 cart 传入 policy，则服务端使用该 policy 计算 totals（否则回退到服务端促销引擎）。
std::string Server::SERsaveCart(const std::string& userPhone, const std::string& cartData) {
    if (!dbManager) { Logger::instance().fail("Server SERsaveCart: dbManager is null"); json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERsaveCart: 数据库未连接"); json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        auto j = json::parse(cartData);
        TemporaryCart cart;
        if (j.contains("cart_id") && j["cart_id"].is_string() && !j["cart_id"].get<std::string>().empty()) cart.cart_id = j["cart_id"].get<std::string>();
        else cart.cart_id = generateCartId(userPhone);
        cart.user_phone = userPhone;
        cart.shipping_address = j.value("shipping_address", std::string(""));
        cart.discount_policy = j.value("discount_policy", std::string(""));
        cart.is_converted = j.value("is_converted", false);
        cart.items.clear();
        if (j.contains("items") && j["items"].is_array()) {
            for (const auto& it : j["items"]) {
                CartItem ci;
                ci.good_id = it.value("good_id", 0);
                ci.good_name = it.value("good_name", std::string(""));
                ci.price = it.value("price", 0.0);
                ci.quantity = it.value("quantity", 0);
                ci.subtotal = it.value("subtotal", ci.price * ci.quantity);
                cart.items.push_back(ci);
            }
        }

        // 提取可能随 cartData 一并发送的 policy（兼容多种字段名）
        nlohmann::json parsedPolicy;
        if (j.contains("policy")) {
            parsedPolicy = j["policy"];
        } else if (j.contains("policy_str")) {
            std::string ps = j.value("policy_str", std::string(""));
            try { parsedPolicy = nlohmann::json::parse(ps); } catch (...) { parsedPolicy = nlohmann::json(); }
        } else if (j.contains("policy_detail")) {
            std::string ps = j.value("policy_detail", std::string(""));
            try { parsedPolicy = nlohmann::json::parse(ps); } catch (...) { parsedPolicy = nlohmann::json(); }
        } else {
            parsedPolicy = nlohmann::json();
        }

        // 使用可能的 policy 进行 recalc（policy 为空时行为与之前一致）
        Server::recalcCartTotalsImpl(cart, parsedPolicy, dbManager);

        // save or update
        if (!dbManager->DTBsaveTemporaryCart(cart)) {
            if (!dbManager->DTBupdateTemporaryCart(cart)) {
                json r; r["error"] = "保存失败"; return r.dump();
            }
        }
        json ok; ok["result"] = "saved"; ok["cart_id"] = cart.cart_id; return ok.dump();
    }
    catch (const std::exception& ex) {
        Logger::instance().fail(std::string("Server SERsaveCart 异常: ") + ex.what());
        json err; err["error"] = "保存失败"; err["message"] = ex.what(); return err.dump();
    }
}

std::string Server::SERaddToCart(const std::string& userPhone, int productId, const std::string& productName, double price, int quantity) {
    if (!dbManager) { Logger::instance().fail("Server SERaddToCart: dbManager is null"); json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERaddToCart: 数据库未连接"); json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        if (quantity <= 0) { json r; r["error"] = "quantity_invalid"; return r.dump(); }
        Good g;
        if (!dbManager->DTBloadGood(productId, g)) { json r; r["error"] = "good_not_found"; r["id"] = productId; return r.dump(); }
        TemporaryCart cart;
        bool exists = dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart);
        int existingQty = 0;
        if (exists) {
            for (const auto& it : cart.items) if (it.good_id == productId) existingQty = it.quantity;
        }
        if ((long long)existingQty + (long long)quantity > (long long)g.getStock()) {
            json r; r["error"] = "stock_exceeded"; r["available"] = g.getStock(); r["requested"] = existingQty + quantity; return r.dump();
        }
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
            CartItem ci; ci.good_id = productId; ci.good_name = productName.empty() ? g.getName() : productName; ci.price = price <= 0.0 ? g.getPrice() : price; ci.quantity = quantity; ci.subtotal = ci.price * ci.quantity;
            cart.items.push_back(ci);
        }
        recalcCartTotals(cart);
        if (exists) {
            if (!dbManager->DTBupdateTemporaryCart(cart)) { json r; r["error"] = "更新购物车失败"; return r.dump(); }
        }
        else {
            if (!dbManager->DTBsaveTemporaryCart(cart)) { json r; r["error"] = "保存购物车失败"; return r.dump(); }
        }
        json ok; ok["result"] = "ok"; ok["cart_id"] = cart.cart_id; return ok.dump();
    }
    catch (const std::exception& ex) {
        Logger::instance().fail(std::string("Server SERaddToCart 异常: ") + ex.what());
        json err; err["error"] = "添加失败"; err["message"] = ex.what(); return err.dump();
    }
}

std::string Server::SERupdateCartItem(const std::string& userPhone, int productId, int quantity) {
    if (!dbManager) { Logger::instance().fail("Server SERupdateCartItem: dbManager is null"); json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERupdateCartItem: 数据库未连接"); json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Logger::instance().info(std::string("Server SERupdateCartItem: request userPhone=") + userPhone + ", productId=" + std::to_string(productId) + ", quantity=" + std::to_string(quantity));
        if (quantity < 0) { json r; r["error"] = "quantity_invalid"; return r.dump(); }
        Good g;
        if (!dbManager->DTBloadGood(productId, g)) { json r; r["error"] = "good_not_found"; return r.dump(); }
        if (quantity > g.getStock()) { json r; r["error"] = "stock_exceeded"; r["available"] = g.getStock(); r["requested"] = quantity; return r.dump(); }
        TemporaryCart cart;
        if (!dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart)) { json r; r["error"] = "未找到购物车"; Logger::instance().warn("Server SERupdateCartItem: 未找到购物车 for userPhone=" + userPhone); return r.dump(); }
        bool found = false;
        for (auto& it : cart.items) {
            if (it.good_id == productId) {
                if (quantity <= 0) {
                    found = true;
                    break;
                }
                it.quantity = quantity;
                it.subtotal = it.price * it.quantity;
                found = true;
                break;
            }
        }
        if (!found) { json r; r["error"] = "未找到商品"; Logger::instance().warn("Server SERupdateCartItem: 未找到商品 productId=" + std::to_string(productId)); return r.dump(); }
        recalcCartTotals(cart);
        if (!dbManager->DTBupdateTemporaryCart(cart)) { json r; r["error"] = "更新失败"; Logger::instance().fail("Server SERupdateCartItem: DTBupdateTemporaryCart 返回 false"); return r.dump(); }
        json ok; ok["result"] = "updated"; ok["cart_id"] = cart.cart_id; return ok.dump();
    }
    catch (const std::exception& ex) {
        Logger::instance().fail(std::string("Server SERupdateCartItem 异常: ") + ex.what());
        json err; err["error"] = "更新失败"; err["message"] = ex.what(); return err.dump();
    }
}

std::string Server::SERremoveFromCart(const std::string& userPhone, int productId) {
    if (!dbManager) { Logger::instance().fail("Server SERremoveFromCart: dbManager is null"); json e; e["error"] = "服务器内部错误"; return e.dump(); }
    if (!dbManager->DTBisConnected() && !dbManager->DTBinitialize()) { Logger::instance().fail("Server SERremoveFromCart: 数据库未连接"); json e; e["error"] = "数据库未连接"; return e.dump(); }
    try {
        Logger::instance().info(std::string("Server SERremoveFromCart: request userPhone=") + userPhone + ", productId=" + std::to_string(productId));
        TemporaryCart cart;
        if (!dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart)) { json r; r["error"] = "未找到购物车"; Logger::instance().warn("Server SERremoveFromCart: 未找到购物车 for userPhone=" + userPhone); return r.dump(); }
        auto it = std::remove_if(cart.items.begin(), cart.items.end(), [productId](const CartItem& ci) { return ci.good_id == productId; });
        if (it == cart.items.end()) { json r; r["error"] = "未找到商品"; Logger::instance().warn("Server SERremoveFromCart: 未找到商品 productId=" + std::to_string(productId)); return r.dump(); }
        cart.items.erase(it, cart.items.end());
        recalcCartTotals(cart);
        if (!dbManager->DTBupdateTemporaryCart(cart)) { json r; r["error"] = "更新失败"; Logger::instance().fail("Server SERremoveFromCart: DTBupdateTemporaryCart 返回 false"); return r.dump(); }
        json ok; ok["result"] = "removed"; ok["cart_id"] = cart.cart_id; Logger::instance().info(std::string("Server SERremoveFromCart: removed productId=") + std::to_string(productId)); return ok.dump();
    }
    catch (const std::exception& ex) {
        Logger::instance().fail(std::string("Server SERremoveFromCart 异常: ") + ex.what());
        json err; err["error"] = "删除失败"; err["message"] = ex.what(); return err.dump();
    }
}

std::string Server::SERgetAllPromotions() {
    if (!dbManager) return std::string("{\"error\":\"db not available\"}");
    auto rows = dbManager->DTBloadAllPromotionStrategies(false);
    json arr = json::array();
    for (const auto& m : rows) {
        json obj;
        obj["id"] = m.count("id") ? m.at("id") : "";
        obj["name"] = m.count("name") ? m.at("name") : "";
        try {
            obj["policy"] = m.count("policy_detail") ? json::parse(m.at("policy_detail")) : json();
        }
        catch (...) {
            obj["policy"] = m.count("policy_detail") ? m.at("policy_detail") : "";
        }
        arr.push_back(obj);
    }
    return arr.dump();
}

// 添加促销：管理员使用。输入 JSON（包含 name 与 policy 字段）
std::string Server::SERaddPromotion(const nlohmann::json& j) {
    if (!dbManager) { nlohmann::json e; e["error"] = "db not available"; return e.dump(); }
    try {
        std::string name = j.value("name", std::string(""));
        if (name.empty()) { nlohmann::json e; e["error"] = "missing_name"; return e.dump(); }
        // policy 可以是对象或字符串，存储为字符串
        std::string policy;
        if (j.contains("policy")) {
            try { policy = j["policy"].is_string() ? j["policy"].get<std::string>() : j["policy"].dump(); }
            catch (...) { policy = j["policy"].dump(); }
        } else policy = j.value("policy_detail", std::string(""));
        std::string type = j.value("type", std::string(""));
        std::string conditions = j.value("conditions", std::string(""));

        if (!dbManager->DTBsavePromotionStrategy(name, type, policy, conditions)) {
            nlohmann::json r; r["error"] = "save_failed"; return r.dump();
        }
        nlohmann::json ok; ok["result"] = "added"; ok["name"] = name; return ok.dump();
    } catch (const std::exception& ex) {
        nlohmann::json err; err["error"] = "exception"; err["message"] = ex.what(); return err.dump();
    }
}

// 更新促销（按 name 更新 policy_detail）
std::string Server::SERupdatePromotion(const nlohmann::json& j) {
    if (!dbManager) { nlohmann::json e; e["error"] = "db not available"; return e.dump(); }
    try {
        std::string name = j.value("name", std::string(""));
        if (name.empty()) { nlohmann::json e; e["error"] = "missing_name"; return e.dump(); }

        std::string newName = j.value("new_name", std::string("")); // 支持重命名：传 new_name 即可
        // 接受 policy 对象或 policy_detail 字符串
        std::string policy;
        if (j.contains("policy")) {
            try { policy = j["policy"].is_string() ? j["policy"].get<std::string>() : j["policy"].dump(); }
            catch (...) { policy = j["policy"].dump(); }
        } else policy = j.value("policy_detail", std::string(""));

        std::string type = j.value("type", std::string(""));
        std::string conditions = j.value("conditions", std::string(""));

        // 如果请求包含 new_name 且与现有 name 不同 -> 执行重命名（同时可更新 policy/type/conditions）
        if (!newName.empty() && newName != name) {
            // 读取现有记录以尽量保全未传入的字段
            std::map<std::string, std::string> existing = dbManager->DTBloadPromotionStrategy(name);
            if (existing.empty()) {
                nlohmann::json err; err["error"] = "not_found"; err["message"] = "原策略未找到"; return err.dump();
            }

            std::string finalPolicy = !policy.empty() ? policy : (existing.count("policy_detail") ? existing.at("policy_detail") : std::string(""));
            std::string finalType = !type.empty() ? type : (existing.count("type") ? existing.at("type") : std::string(""));
            std::string finalConditions = !conditions.empty() ? conditions : (existing.count("conditions") ? existing.at("conditions") : std::string(""));

            // 尝试保存新名字的策略
            if (!dbManager->DTBsavePromotionStrategy(newName, finalType, finalPolicy, finalConditions)) {
                nlohmann::json err; err["error"] = "save_new_failed"; err["message"] = "保存新策略名失败"; return err.dump();
            }
            // 尝试删除旧条目（若删除失败，仅记录警告，但返回成功）
            if (!dbManager->DTBdeletePromotionStrategy(name)) {
                Logger::instance().warn("Server SERupdatePromotion: rename succeeded but failed to delete old promotion: " + name);
            }
            nlohmann::json ok; ok["result"] = "renamed"; ok["old_name"] = name; ok["new_name"] = newName; return ok.dump();
        }

        // 否则按原来逻辑更新 policy_detail（如果更新失败则尝试保存）
        if (!dbManager->DTBupdatePromotionStrategyDetail(name, policy)) {
            if (!dbManager->DTBsavePromotionStrategy(name, type, policy, conditions)) {
                nlohmann::json r; r["error"] = "update_failed"; return r.dump();
            }
        }
        nlohmann::json ok; ok["result"] = "updated"; ok["name"] = name; return ok.dump();
    } catch (const std::exception& ex) {
        nlohmann::json err; err["error"] = "exception"; err["message"] = ex.what(); return err.dump();
    } catch (...) {
        nlohmann::json err; err["error"] = "unknown_exception"; return err.dump();
    }
}

// 删除促销（按 name）
std::string Server::SERdeletePromotion(const std::string& name) {
    if (!dbManager) { nlohmann::json e; e["error"] = "db not available"; return e.dump(); }
    try {
        if (!dbManager->DTBdeletePromotionStrategy(name)) {
            nlohmann::json r; r["error"] = "delete_failed"; return r.dump();
        }
        nlohmann::json ok; ok["result"] = "deleted"; ok["name"] = name; return ok.dump();
    } catch (const std::exception& ex) {
        nlohmann::json err; err["error"] = "exception"; err["message"] = ex.what(); return err.dump();
    }
}

// --------- Promotions helpers: implementations added to resolve linker errors ----------
std::string Server::SERgetPromotionsByProductId(int productId) {
    if (!dbManager) {
        Logger::instance().warn("Server SERgetPromotionsByProductId: dbManager is null");
        return std::string("{\"error\":\"db not available\"}");
    }
    try {
        auto rows = dbManager->DTBloadAllPromotionStrategies(false);
        json arr = json::array();
        for (const auto& m : rows) {
            try {
                if (!m.count("policy_detail")) continue;
                json p = json::parse(m.at("policy_detail"));
                bool applies = false;
                // 全局适用
                if (p.contains("scope") && p["scope"].is_string() && p["scope"] == "global") applies = true;
                // 按 product_ids 匹配
                if (p.contains("product_ids") && p["product_ids"].is_array()) {
                    for (const auto& pid : p["product_ids"]) {
                        if (pid.is_number() && pid.get<int>() == productId) { applies = true; break; }
                        if (pid.is_string()) {
                            try { if (std::stoi(pid.get<std::string>()) == productId) { applies = true; break; } }
                            catch (...) {}
                        }
                    }
                }
                if (applies) {
                    json obj;
                    obj["id"] = m.count("id") ? m.at("id") : "";
                    obj["name"] = m.count("name") ? m.at("name") : "";
                    obj["policy"] = p;
                    arr.push_back(obj);
                }
            } catch (const std::exception& ex) {
                Logger::instance().warn(std::string("Server SERgetPromotionsByProductId: skip policy parse error: ") + ex.what());
                continue;
            } catch (...) {
                continue;
            }
        }
        return arr.dump();
    } catch (const std::exception& ex) {
        Logger::instance().fail(std::string("Server SERgetPromotionsByProductId 异常: ") + ex.what());
        json err; err["error"] = "exception"; err["message"] = ex.what(); return err.dump();
    } catch (...) {
        json err; err["error"] = "unknown_exception"; return err.dump();
    }
}

std::string Server::SERupdateCartForPromotions(const std::string& userPhone) {
    if (!dbManager) {
        Logger::instance().warn("Server SERupdateCartForPromotions: dbManager is null");
        return std::string("{\"error\":\"db not available\"}");
    }

    TemporaryCart cart;
    if (!dbManager->DTBloadTemporaryCartByUserPhone(userPhone, cart)) {
        Logger::instance().info("Server SERupdateCartForPromotions: cart not found for userPhone=" + userPhone);
        return std::string("{\"error\":\"cart_not_found\"}");
    }

    double original_total = cart.total_amount;
    double new_total = 0.0;

    for (auto& it : cart.items) {
        double bestSubtotal = it.price * static_cast<double>(it.quantity);
        try {
            std::string promoResp = SERgetPromotionsByProductId(it.good_id);
            json parr = json::parse(promoResp);
            if (parr.is_array()) {
                for (const auto& p : parr) {
                    try {
                        json policy = p.contains("policy") ? p["policy"] : json::object();
                        auto strat = PromotionStrategy::fromJson(policy);
                        double applied = strat->apply(it.price, it.quantity);
                        if (applied < bestSubtotal) bestSubtotal = applied;
                    } catch (...) {
                        // 单条策略解析/应用失败则跳过
                        continue;
                    }
                }
            }
        } catch (...) {
            // 如果 promotions 查询或解析失败，默认使用原始小计
            Logger::instance().warn("Server SERupdateCartForPromotions: failed to evaluate promotions for good_id=" + std::to_string(it.good_id));
        }
        new_total += bestSubtotal;
    }

    cart.discount_amount = std::max(0.0, original_total - new_total);
    cart.final_amount = new_total;
    cart.total_amount = original_total;

    // 尝试更新数据库，失败则尝试插入
    if (!dbManager->DTBupdateTemporaryCart(cart)) {
        if (!dbManager->DTBsaveTemporaryCart(cart)) {
            Logger::instance().fail("Server SERupdateCartForPromotions: failed to persist updated cart for user " + userPhone);
            json err; err["error"] = "persist_failed"; return err.dump();
        }
    }

    json resp;
    resp["result"] = "updated";
    resp["original_total"] = original_total;
    resp["final_amount"] = cart.final_amount;
    resp["discount_amount"] = cart.discount_amount;
    Logger::instance().info("Server SERupdateCartForPromotions: updated cart for userPhone=" + userPhone + " original=" + std::to_string(original_total) + " final=" + std::to_string(cart.final_amount));
    return resp.dump();
}

// 新增：根据 policy JSON 计算折后金额（放到 Server.cpp 顶部的辅助函数区）
static double computeFinalFromPolicy(const nlohmann::json& policy, const nlohmann::json& items, double originalTotal) {
    if (!policy.is_object()) return originalTotal;
    std::string type = policy.value("type", std::string(""));
    if (type == "discount") {
        double d = policy.value("discount", 1.0);
        return originalTotal * d;
    } else if (type == "full_reduction") {
        double threshold = policy.value("threshold", 0.0);
        double reduce = policy.value("reduce", 0.0);
        if (originalTotal >= threshold) return std::max(0.0, originalTotal - reduce);
        return originalTotal;
    } else if (type == "tiered") {
        // 计算总数量
        int totalQty = 0;
        for (const auto& it : items) {
            totalQty += it.value("quantity", 0);
        }
        double applied = 1.0;
        if (policy.contains("tiers") && policy["tiers"].is_array()) {
            for (const auto &tier : policy["tiers"]) {
                int minq = tier.value("min_qty", 0);
                double disc = tier.value("discount", 1.0);
                if (totalQty >= minq) applied = disc;
            }
        }
        return originalTotal * applied;
    }
    return originalTotal;
}

// 辅助：把 policy JSON 转成可读名称（如果客户端没有传名字）
// 简单映射：discount -> "X%off", full_reduction -> "满<thr>减<red>"
static std::string derivePolicyName(const nlohmann::json& policy) {
    if (!policy.is_object()) return std::string();
    std::string type = policy.value("type", std::string(""));
    if (type == "discount") {
        double d = policy.value("discount", 1.0);
        int pct = int(d * 100 + 0.5);
        return std::to_string(pct) + "%off";
    } else if (type == "full_reduction") {
        double thr = policy.value("threshold", 0.0);
        double red = policy.value("reduce", 0.0);
        std::ostringstream oss;
        oss << "满" << (long)thr << "减" << (long)red;
        return oss.str();
    } else if (type == "tiered") {
        return "tiered";
    }
    return std::string();
}

// ---------- 新增：基于 policy 的 recalc 实现（文件作用域 helper） ----------
/*
  修改点：
  - 增加第三个参数 DatabaseManager* dbManager（默认 nullptr），
    以便在文件作用域函数中访问数据库中的促销策略（避免使用 this 或调用非静态成员）。
  - 在没有 dbManager 时保留回退行为（不应用任何促销）。
*/

// ---------- 替换：Server::recalcCartTotals 调用到文件作用域实现 ----------
void Server::recalcCartTotals(TemporaryCart& cart) {
    // 默认不使用客户端 policy（保留旧行为）
    // 将当前实例的 dbManager 传入文件作用域实现，避免在文件作用域使用 this
    Server::recalcCartTotalsImpl(cart, nlohmann::json(), dbManager);
}

// 添加：确保目录存在的实现（放在文件顶部辅助函数区，靠近其他静态辅助函数）


// 添加：实现 Server::recalcCartTotalsImpl，桥接到文件作用域实现 recalcCartTotals
void Server::recalcCartTotalsImpl(TemporaryCart& cart, const nlohmann::json& policy, DatabaseManager* dbManager) {
    // 调用局部实现，保留原有逻辑
    recalcCartTotalsImpl_local(cart, policy, dbManager);
}
