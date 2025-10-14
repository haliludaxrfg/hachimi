#include "Client.h"
#include <nlohmann/json.hpp>
#include "logger.h"
#include <QElapsedTimer>
#include <QCoreApplication>
#include <sstream>


using nlohmann::json;

class Client::Impl {
public:
    QString ip;
    int port;
    QTcpSocket* socket;
    bool connected;

    Impl(const std::string& ip_, int port_)
        : ip("127.0.0.1"), port(8888), socket(new QTcpSocket), connected(false) {} // 强制使用127.0.0.1:8888

    ~Impl() {
        if (socket) {
            socket->disconnectFromHost();
            socket->waitForDisconnected(1000);
            delete socket;
        }
    }
};

Client::Client(const std::string& ip, int port)
    : pImpl(new Impl("127.0.0.1", 8888)) {} // 强制使用127.0.0.1:8888

Client::~Client() {
    CLTdisconnect();
    delete pImpl;
}

bool Client::CLTconnectToServer() {
    QMutexLocker lock(&requestMutex);
    if (pImpl->connected) return true;

    pImpl->socket->connectToHost(QHostAddress(pImpl->ip), pImpl->port);
    if (!pImpl->socket->waitForConnected(3000)) {
        qWarning() << "Connect failed, error:" << pImpl->socket->errorString();
        pImpl->connected = false;
        return false;
    }
    pImpl->connected = true;
    return true;
}

bool Client::CLTreconnect() {
    QMutexLocker lock(&requestMutex);
    // 先尝试断开连接
    if (pImpl->connected) {
        pImpl->socket->disconnectFromHost();
        if (pImpl->socket->state() == QAbstractSocket::ConnectedState) {
            pImpl->socket->waitForDisconnected(3000);
        }
        else {
            qDebug() << "Cannot waitForDisconnected in state:" << pImpl->socket->state();
        }
        pImpl->connected = false;
    }
    // 尝试重新连接
    pImpl->socket->connectToHost(QHostAddress(pImpl->ip), pImpl->port);
    if (!pImpl->socket->waitForConnected(5000)) {
        qDebug() << "Connection failed:" << pImpl->socket->errorString();
        Logger::instance().fail("Socket连接失败: " + pImpl->socket->errorString().toStdString());
        return false;
    }
    pImpl->connected = true;
    return true;
}

void Client::CLTdisconnect() {
    QMutexLocker lock(&requestMutex);
    if (pImpl->connected) {
        pImpl->socket->disconnectFromHost();
        if (pImpl->socket->state() == QAbstractSocket::ConnectedState) {
            pImpl->socket->waitForDisconnected(3000);
        } else {
            qDebug() << "Cannot waitForDisconnected in state:" << pImpl->socket->state();
        }
        pImpl->connected = false;
    }
}

bool Client::CLTisConnectionActive() const {
    return pImpl->connected;
}

std::string Client::CLTsendRequest(const std::string& request) {
    QMutexLocker lock(&requestMutex);

    auto trim = [](const std::string& s) {
        const char* ws = " \t\n\r";
        size_t b = s.find_first_not_of(ws);
        if (b == std::string::npos) return std::string();
        size_t e = s.find_last_not_of(ws);
        return s.substr(b, e - b + 1);
        };

    std::string reqStr = trim(request);
    if (reqStr.empty()) {
        qWarning() << "CLTsendRequest: empty request";
        return "";
    }

    // normalize and ensure single newline terminator
    std::string formatted = reqStr;
    if (formatted.back() != '\n') formatted.push_back('\n');

    if (!pImpl->connected) {
        qWarning() << "Not connected to server";
        return "";
    }

    QByteArray qreq = QByteArray::fromStdString(formatted);

    // try write, on failure try one reconnect+retry
    qint64 sent = -1;
    for (int attempt = 0; attempt < 2; ++attempt) {
        sent = pImpl->socket->write(qreq);
        qDebug() << "CLTsendRequest: write returned" << sent << "(attempt" << attempt + 1 << ")";
        if (sent == -1) {
            qWarning() << "Send failed, socket error:" << pImpl->socket->errorString();
            // lightweight reconnect
            pImpl->socket->disconnectFromHost();
            if (pImpl->socket->state() == QAbstractSocket::ConnectedState) pImpl->socket->waitForDisconnected(1000);
            pImpl->connected = false;
            pImpl->socket->connectToHost(QHostAddress(pImpl->ip), pImpl->port);
            if (!pImpl->socket->waitForConnected(3000)) {
                qDebug() << "Reconnect attempt failed:" << pImpl->socket->errorString();
                continue;
            }
            pImpl->connected = true;
            continue;
        }
        break;
    }

    if (sent == -1) {
        qWarning() << "CLTsendRequest: all send attempts failed";
        return "";
    }

    if (!pImpl->socket->waitForBytesWritten(2000)) {
        qWarning() << "waitForBytesWritten timed out or failed:" << pImpl->socket->errorString();
    }

    QByteArray resp;
    const int timeLimitMs = 8000;
    const int chunkMs = 200;
    QElapsedTimer timer; timer.start();

    while (timer.elapsed() < timeLimitMs) {
        if (pImpl->socket->bytesAvailable() > 0) {
            resp += pImpl->socket->readAll();
            while (pImpl->socket->waitForReadyRead(100)) resp += pImpl->socket->readAll();
            break;
        }
        if (pImpl->socket->waitForReadyRead(chunkMs)) {
            resp += pImpl->socket->readAll();
            while (pImpl->socket->waitForReadyRead(100)) resp += pImpl->socket->readAll();
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    if (resp.isEmpty()) {
        qWarning("No response from server after total wait");
        qDebug() << "final socket state:" << pImpl->socket->state() << "error:" << pImpl->socket->errorString();
        return "";
    }

    QString s = QString::fromUtf8(resp);
    QString trimmed = s.trimmed();
    qDebug() << "CLTsendRequest: request:" << QString::fromStdString(formatted).trimmed();
    qDebug() << "CLTsendRequest: received (len)" << resp.size() << ", trimmed len:" << trimmed.toUtf8().size();

    return trimmed.toStdString();
}

// ---------------- 商品相关 ----------------
std::vector<Good> Client::CLTgetAllGoods() {
    std::vector<Good> goods;
    std::string resp = CLTsendRequest("GET_ALL_GOODS");
    if (resp.empty()) return goods;
    try {
        auto j = json::parse(resp);
        if (j.is_object() && j.contains("error")) {
            Logger::instance().fail("GET_ALL_GOODS 返回错误: " + j.dump());
            return goods;
        }
        json arr = j.is_array() ? j : (j.contains("data") && j["data"].is_array() ? j["data"] : json::array());
        for (const auto& it : arr) {
            if (!it.is_object()) continue;
            try {
                int id = it.value("id", 0);
                std::string name = it.value("name", std::string(""));
                double price = it.value("price", 0.0);
                int stock = it.value("stock", 0);
                std::string category = it.value("category", std::string(""));
                goods.emplace_back(id, name, price, stock, category);
            }
            catch (...) {
                Logger::instance().warn("解析商品条目失败: " + it.dump());
            }
        }
    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("GET_ALL_GOODS 解析失败: ") + e.what());
    }
    return goods;
}

bool Client::CLTupdateGood(int id, const std::string& name, double price, int stock, const std::string& category) {
    json j;
    j["id"] = id; j["name"] = name; j["price"] = price; j["stock"] = stock; j["category"] = category;
    std::string resp = CLTsendRequest(std::string("UPDATE_GOOD ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) {
            Logger::instance().fail("UPDATE_GOOD 返回错误: " + r.dump());
            return false;
        }
        return true;
    }
    catch (...) {
        Logger::instance().fail("UPDATE_GOOD 响应解析失败");
        return false;
    }
}

bool Client::CLTaddGood(const std::string& name, double price, int stock, const std::string& category) {
    json j;
    j["name"] = name; j["price"] = price; j["stock"] = stock; j["category"] = category;
    std::string resp = CLTsendRequest(std::string("ADD_GOOD ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) {
            Logger::instance().fail("ADD_GOOD 返回错误: " + r.dump());
            return false;
        }
        return true;
    }
    catch (...) {
        Logger::instance().fail("ADD_GOOD 响应解析失败");
        return false;
    }
}

bool Client::CLTgetGoodById(int id, Good& outGood) {
    std::string resp = CLTsendRequest(std::string("GET_GOOD_BY_ID ") + std::to_string(id));
    if (resp.empty()) return false;
    try {
        auto j = json::parse(resp);
        if (j.is_object() && j.contains("error")) {
            Logger::instance().fail("GET_GOOD_BY_ID 返回错误: " + j.dump());
            return false;
        }
        if (!j.is_object()) return false;
        outGood = Good(j.value("id", 0), j.value("name", std::string("")), j.value("price", 0.0), j.value("stock", 0), j.value("category", std::string("")));
        return true;
    }
    catch (...) {
        Logger::instance().fail("GET_GOOD_BY_ID 解析失败");
        return false;
    }
}

bool Client::CLTdeleteGood(int id) {
    std::string resp = CLTsendRequest(std::string("DELETE_GOOD ") + std::to_string(id));
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "deleted";
    }
    catch (...) {
        Logger::instance().fail("DELETE_GOOD 解析失败");
        return false;
    }
}

std::vector<Good> Client::CLTsearchGoodsByCategory(const std::string& category) {
    std::vector<Good> goods;
    json j; j["category"] = category;
    std::string resp = CLTsendRequest(std::string("SEARCH_GOODS_BY_CATEGORY ") + j.dump());
    if (resp.empty()) return goods;
    try {
        auto arr = json::parse(resp);
        if (arr.is_object() && arr.contains("error")) {
            Logger::instance().fail("SEARCH_GOODS_BY_CATEGORY 返回错误: " + arr.dump());
            return goods;
        }
        json src = arr.is_array() ? arr : (arr.contains("data") && arr["data"].is_array() ? arr["data"] : json::array());
        for (const auto& it : src) {
            try {
                goods.emplace_back(it.value("id", 0), it.value("name", std::string("")), it.value("price", 0.0), it.value("stock", 0), it.value("category", std::string("")));
            }
            catch (...) {
                Logger::instance().warn("解析商品条目失败: " + it.dump());
            }
        }
    }
    catch (...) {
        Logger::instance().fail("SEARCH_GOODS_BY_CATEGORY 解析失败");
    }
    return goods;
}

// ---------------- 用户相关 ----------------
std::vector<User> Client::CLTgetAllAccounts() {
    std::vector<User> users;
    std::string resp = CLTsendRequest("GET_ALL_ACCOUNTS");
    if (resp.empty()) return users;
    try {
        auto arr = json::parse(resp);
        if (!arr.is_array()) { Logger::instance().fail("GET_ALL_ACCOUNTS 响应不是数组"); return users; }
        for (const auto& it : arr) {
            try {
                users.emplace_back(it.value("phone", std::string("")), it.value("password", std::string("")), it.value("address", std::string("")));
            }
            catch (...) {
                Logger::instance().warn("解析用户条目失败: " + it.dump());
            }
        }
    }
    catch (...) {
        Logger::instance().fail("GET_ALL_ACCOUNTS 解析失败");
    }
    return users;
}

bool Client::CLTlogin(const std::string& phone, const std::string& password) {
    json j; j["phone"] = phone; j["password"] = password;
    std::string resp = CLTsendRequest(std::string("LOGIN ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) { Logger::instance().fail("LOGIN 返回错误: " + r.dump()); return false; }
        return r.value("result", std::string("")) == "ok";
    }
    catch (...) {
        Logger::instance().fail("LOGIN 解析失败");
        return false;
    }
}

bool Client::CLTupdateAccountPassword(const std::string& userId, const std::string& oldPassword, const std::string& newPassword) {
    json j; j["userId"] = userId; j["oldPassword"] = oldPassword; j["newPassword"] = newPassword;
    std::string resp = CLTsendRequest(std::string("UPDATE_ACCOUNT_PASSWORD ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) { Logger::instance().fail("UPDATE_ACCOUNT_PASSWORD 返回错误: " + r.dump()); return false; }
        return r.value("result", std::string("")) == "updated";
    }
    catch (...) {
        Logger::instance().fail("UPDATE_ACCOUNT_PASSWORD 解析失败");
        return false;
    }
}

bool Client::CLTdeleteAccount(const std::string& phone, const std::string& password) {
    json j; j["phone"] = phone; j["password"] = password;
    std::string resp = CLTsendRequest(std::string("DELETE_ACCOUNT ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "deleted";
    }
    catch (...) {
        Logger::instance().fail("DELETE_ACCOUNT 解析失败");
        return false;
    }
}

bool Client::CLTaddAccount(const std::string& phone, const std::string& password,const std::string& address) {
	json j; j["phone"] = phone; j["password"] = password; j["address"] = address;
    std::string resp = CLTsendRequest(std::string("ADD_ACCOUNT ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "added";
    }
    catch (...) {
        Logger::instance().fail("ADD_ACCOUNT 解析失败");
        return false;
    }
}

bool Client::CLTupdateUser(const std::string& phone, const std::string& password, const std::string& address) {
    try {
        nlohmann::json j;
        j["phone"] = phone;
        j["password"] = password;
        j["address"] = address;
        std::string req = std::string("UPDATE_USER ") + j.dump();
        std::string resp = CLTsendRequest(req);
        if (resp.empty()) return false;
        // 视为成功如果没有 error 字段
        return resp.find("\"error\"") == std::string::npos;
    }
    catch (...) {
        return false;
    }
}

// ---------------- 购物车相关 ----------------
TemporaryCart Client::CLTgetCartForUser(const std::string& userPhone) {
    TemporaryCart cart;
    // 记录请求
    Logger::instance().info(std::string("CLTgetCartForUser: request for userPhone=") + userPhone);
    std::string resp = CLTsendRequest(std::string("GET_CART ") + userPhone);
    if (resp.empty()) {
        Logger::instance().info(std::string("CLTgetCartForUser: empty response for userPhone=") + userPhone);
        return cart;
    }
    // 记录原始响应以便排查
    Logger::instance().info(std::string("CLTgetCartForUser: raw response: ") + resp);
    try {
        auto j = json::parse(resp);
        // 若包含 error，则记录并返回空 cart
        if (j.is_object() && j.contains("error")) {
            Logger::instance().warn(std::string("CLTgetCartForUser: server returned error: ") + j.dump());
            return cart;
        }
        cart.cart_id = j.value("cart_id", std::string(""));
        cart.user_phone = j.value("user_phone", std::string(""));
        cart.shipping_address = j.value("shipping_address", std::string(""));
        cart.discount_policy = j.value("discount_policy", std::string(""));
        cart.total_amount = j.value("total_amount", 0.0);
        cart.discount_amount = j.value("discount_amount", 0.0);
        cart.final_amount = j.value("final_amount", 0.0);
        cart.is_converted = j.value("is_converted", false);
        cart.items.clear();
        if (j.contains("items") && j["items"].is_array()) {
            for (const auto& it : j["items"]) {
                try {
                    CartItem tmp;
                    tmp.good_id = it.value("good_id", 0);
                    tmp.good_name = it.value("good_name", std::string(""));
                    tmp.price = it.value("price", 0.0);
                    tmp.quantity = it.value("quantity", 0);
                    tmp.subtotal = it.value("subtotal", tmp.price * tmp.quantity);
                    cart.items.push_back(tmp);
                }
                catch (...) {
                    Logger::instance().warn("CLTgetCartForUser: 解析购物车项失败: " + it.dump());
                }
            }
        }
        // 记录解析结果
        Logger::instance().info(std::string("CLTgetCartForUser: parsed cart_id=") + cart.cart_id + ", items=" + std::to_string(cart.items.size()));
        for (size_t i = 0; i < cart.items.size(); ++i) {
            const auto& it = cart.items[i];
            Logger::instance().info(std::string("CLTgetCartForUser: item[") + std::to_string(i) + "] good_id=" + std::to_string(it.good_id) +
                                   ", name=" + it.good_name + ", price=" + std::to_string(it.price) + ", qty=" + std::to_string(it.quantity) +
                                   ", subtotal=" + std::to_string(it.subtotal));
        }
    }
    catch (const std::exception& e) {
        Logger::instance().fail(std::string("CLTgetCartForUser 解析失败: ") + e.what());
    }
    return cart;
}

bool Client::CLTsaveCartForUserWithPolicy(const TemporaryCart& cart, const std::string& policyJson) {
    try {
        nlohmann::json j;
        // 假定 TemporaryCart 能序列化为 JSON（若没有，请按字段组装）
        nlohmann::json cartJson;
        cartJson["cart_id"] = cart.cart_id;
        cartJson["discount_amount"] = cart.discount_amount;
        cartJson["discount_policy"] = cart.discount_policy;
        cartJson["final_amount"] = cart.final_amount;
        cartJson["is_converted"] = cart.is_converted;
        cartJson["shipping_address"] = cart.shipping_address;
        cartJson["total_amount"] = cart.total_amount;
        cartJson["user_phone"] = cart.user_phone;
        // items
        cartJson["items"] = nlohmann::json::array();
        for (const auto &it : cart.items) {
            nlohmann::json item;
            item["good_id"] = it.good_id;
            item["good_name"] = it.good_name;
            item["price"] = it.price;
            item["quantity"] = it.quantity;
            item["subtotal"] = it.subtotal;
            cartJson["items"].push_back(item);
        }

        // 把 policy JSON 放到 cartJson["policy"]（如果 policyJson 非空）
        if (!policyJson.empty()) {
            try {
                cartJson["policy"] = nlohmann::json::parse(policyJson);
            } catch (...) {
                cartJson["policy"] = policyJson; // 保底字符串
            }
        }

        j["cart"] = cartJson;
        j["userPhone"] = cart.user_phone;

        // 使用服务端支持的命令名 SAVE_CART
        std::string req = std::string("SAVE_CART ") + j.dump();
        std::string resp = CLTsendRequest(req);
        if (resp.empty()) return false;
        auto r = nlohmann::json::parse(resp);
        return r.value("result", std::string("")) == "saved";
    } catch (const std::exception& ex) {
        Logger::instance().fail(std::string("CLTsaveCartForUserWithPolicy exception: ") + ex.what());
        return false;
    }
}

bool Client::CLTaddToCart(const std::string& userPhone, int productId, const std::string& productName, double price, int quantity) {
    json j;
    j["userPhone"] = userPhone;
    j["productId"] = productId;
    j["productName"] = productName;
    j["price"] = price;
    j["quantity"] = quantity;
    std::string resp = CLTsendRequest(std::string("ADD_TO_CART ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) { Logger::instance().fail("ADD_TO_CART 返回错误: " + r.dump()); return false; }
        return r.value("result", std::string("")) == "ok";
    }
    catch (...) {
        Logger::instance().fail("ADD_TO_CART 解析失败");
        return false;
    }
}

bool Client::CLTupdateCartItem(const std::string& userPhone, int productId, int quantity) {
    json j; j["userPhone"] = userPhone; j["productId"] = productId; j["quantity"] = quantity;
    std::string resp = CLTsendRequest(std::string("UPDATE_CART_ITEM ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "updated";
    }
    catch (...) {
        Logger::instance().fail("UPDATE_CART_ITEM 解析失败");
        return false;
    }
}

bool Client::CLTremoveFromCart(const std::string& userPhone, int productId) {
    json j; j["userPhone"] = userPhone; j["productId"] = productId;
    std::string resp = CLTsendRequest(std::string("REMOVE_FROM_CART ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "removed";
    }
    catch (...) {
        Logger::instance().fail("REMOVE_FROM_CART 解析失败");
        return false;
    }
}

bool Client::CLTupdateCartForPromotions(const std::string& userPhone) {
    std::string resp = CLTsendRequest(std::string("UPDATE_CART_FOR_PROMOTIONS ") + userPhone);
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) { Logger::instance().fail("UPDATE_CART_FOR_PROMOTIONS 返回错误: " + r.dump()); return false; }
        return true;
    }
    catch (...) {
        Logger::instance().fail("UPDATE_CART_FOR_PROMOTIONS 解析失败");
        return false;
    }
}

// ---------------- 订单相关 ----------------
bool Client::CLTaddSettledOrder(const Order& order) {
    try {
        nlohmann::json payload;
        payload["orderId"] = order.getOrderId();
        payload["userPhone"] = order.getUserPhone();
        payload["status"] = order.getStatus();
        payload["discountPolicy"] = order.getDiscountPolicy();
        // 把金额字段显式传给服务器，避免服务端忽略客户端计算结果
        payload["total_amount"] = order.getTotalAmount();
        payload["discount_amount"] = order.getDiscountAmount();
        payload["final_amount"] = order.getFinalAmount();

        // 加上收货地址
        payload["shipping_address"] = order.getShippingAddress();

        payload["items"] = nlohmann::json::array();
        for (const auto& it : order.getItems()) {
            nlohmann::json ji;
            ji["productId"] = it.getGoodId();
            ji["productName"] = it.getGoodName();
            ji["price"] = it.getPrice();
            ji["quantity"] = it.getQuantity();
            ji["subtotal"] = it.getSubtotal();
            payload["items"].push_back(ji);
        }

        // 发送为文本命令：与服务端其它命令一致的格式
        std::string respStr = CLTsendRequest(std::string("ADD_SETTLED_ORDER ") + payload.dump());
        if (respStr.empty()) return false;

        auto resp = nlohmann::json::parse(respStr);
        if (resp.is_object() && resp.contains("result")) {
            std::string r = resp.value("result", std::string(""));
            return (r == "added" || r == "ok" || r == "saved");
        }
    }
    catch (const std::exception& ex) {
        Logger::instance().fail(std::string("CLTaddSettledOrder exception: ") + ex.what());
    }
    catch (...) {
        Logger::instance().fail("CLTaddSettledOrder unknown exception");
    }
    return false;
}

bool Client::CLTaddSettledOrderRaw(const std::string& orderId, const std::string& productName, int productId,
    int quantity, const std::string& userPhone, int status, const std::string& discountPolicy) {
    json j;
    j["orderId"] = orderId;
    j["productName"] = productName;
    j["productId"] = productId;
    j["quantity"] = quantity;
    j["userPhone"] = userPhone;
    j["status"] = status;
    j["discountPolicy"] = discountPolicy;

    // 尝试从用户购物车读取 shipping_address（若有）
    try {
        TemporaryCart cart = CLTgetCartForUser(userPhone);
        j["shipping_address"] = cart.shipping_address;
    }
    catch (...) {
        j["shipping_address"] = std::string("");
    }

    std::string resp = CLTsendRequest(std::string("ADD_SETTLED_ORDER ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "added";
    }
    catch (...) {
        Logger::instance().fail("ADD_SETTLED_ORDER_RAW 解析失败");
        return false;
    }
}

std::vector<Order> Client::CLTgetAllOrders(const std::string& userPhone) {
    std::vector<Order> out;
    json j; j["userPhone"] = userPhone;
    std::string resp = CLTsendRequest(std::string("GET_ALL_ORDERS ") + (userPhone.empty() ? std::string("{}") : j.dump()));
    if (resp.empty()) return out;
    try {
        auto arr = json::parse(resp);
        if (!arr.is_array()) { Logger::instance().fail("GET_ALL_ORDERS 响应不是数组"); return out; }
        for (const auto& e : arr) {
            std::string oid = e.value("order_id", e.value("orderId", std::string("")));
            int status = e.value("status", 0);
            double final_amount = e.value("final_amount", e.value("finalAmount", 0.0));
            double total_amount = e.value("total_amount", e.value("totalAmount", 0.0));
            std::string user_phone = e.value("user_phone", e.value("userPhone", std::string("")));
            // 新增：尝试读取 shipping_address（兼容 snake_case / camelCase）
            std::string shipping_address = e.value("shipping_address", e.value("shippingAddress", std::string("")));
            // 兼容不同字段名：如果服务器只返回 final_amount，则将其作为 total_amount 的回退值
            if ((total_amount == 0.0) && (final_amount != 0.0)) {
                total_amount = final_amount;
            }

            Order o(TemporaryCart(), oid, status);
            o.setFinalAmount(final_amount);
            o.setTotalAmount(total_amount);
            if (!user_phone.empty()) o.setUserPhone(user_phone);
            if (!shipping_address.empty()) o.setShippingAddress(shipping_address);

            // 尝试解析 items（如果服务器在列表响应中包含简要的 items 信息）
            std::vector<OrderItem> items;
            if (e.contains("items") && e["items"].is_array()) {
                for (const auto& it : e["items"]) {
                    try {
                        OrderItem oi;
                        // 兼容不同字段命名（good_id / productId，good_name / productName）
                        if (it.contains("good_id")) oi.setGoodId(it.value("good_id", 0));
                        else if (it.contains("productId")) oi.setGoodId(it.value("productId", 0));
                        if (it.contains("good_name")) oi.setGoodName(it.value("good_name", std::string("")));
                        else if (it.contains("productName")) oi.setGoodName(it.value("productName", std::string("")));
                        // 价格/数量/小计
                        double price = it.value("price", 0.0);
                        int qty = it.value("quantity", it.value("qty", 0));
                        double subtotal = it.value("subtotal", price * qty);
                        oi.setPrice(price);
                        oi.setQuantity(qty);
                        oi.setSubtotal(subtotal);
                        oi.setOrderId(oid);
                        items.push_back(oi);
                    }
                    catch (...) {
                        Logger::instance().warn("GET_ALL_ORDERS: 解析某个订单项失败");
                        continue;
                    }
                }
            }
            else {
                // 有时服务器返回 items_count / count 字段而非数组，尝试读取
                int count = 0;
                if (e.contains("items_count")) count = e.value("items_count", 0);
                else if (e.contains("count")) count = e.value("count", 0);
                if (count > 0) {
                    items.reserve(count);
                    for (int i = 0; i < count; ++i) {
                        OrderItem oi;
                        oi.setOrderId(oid);
                        items.push_back(oi);
                    }
                }
            }

            if (!items.empty()) o.setItems(items);
            out.push_back(o);
        }
    }
    catch (...) {
        Logger::instance().fail("GET_ALL_ORDERS 解析失败");
    }
    return out;
}

bool Client::CLTgetOrderDetail(const std::string& orderId, const std::string& userPhone, Order& outOrder) {
    json j; j["orderId"] = orderId; j["userPhone"] = userPhone;
    std::string resp = CLTsendRequest(std::string("GET_ORDER_DETAIL ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto jo = json::parse(resp);
        if (jo.is_object() && jo.contains("error")) { Logger::instance().fail("GET_ORDER_DETAIL 返回错误: " + jo.dump()); return false; }
        // 填充 Order
        outOrder = Order(); // reset
        if (jo.contains("order_id")) outOrder.setOrderId(jo.value("order_id", std::string("")));
        if (jo.contains("user_phone")) outOrder.setUserPhone(jo.value("user_phone", std::string("")));
        if (jo.contains("shipping_address")) outOrder.setShippingAddress(jo.value("shipping_address", std::string("")));
        if (jo.contains("status")) outOrder.setStatus(jo.value("status", 0));
        if (jo.contains("total_amount")) outOrder.setTotalAmount(jo.value("total_amount", 0.0));
        if (jo.contains("discount_amount")) outOrder.setDiscountAmount(jo.value("discount_amount", 0.0));
        if (jo.contains("final_amount")) outOrder.setFinalAmount(jo.value("final_amount", 0.0));
        if (jo.contains("discount_policy")) outOrder.setDiscountPolicy(jo.value("discount_policy", std::string("")));
        // items
        std::vector<OrderItem> items;
        if (jo.contains("items") && jo["items"].is_array()) {
            for (const auto& it : jo["items"]) {
                OrderItem oi;
                if (it.contains("good_id")) oi.setGoodId(it.value("good_id", 0));
                if (it.contains("good_name")) oi.setGoodName(it.value("good_name", std::string("")));
                if (it.contains("price")) oi.setPrice(it.value("price", 0.0));
                if (it.contains("quantity")) oi.setQuantity(it.value("quantity", 0));
                if (it.contains("subtotal")) oi.setSubtotal(it.value("subtotal", 0.0));
                oi.setOrderId(orderId);
                items.push_back(oi);
            }
        }
        outOrder.setItems(items);
        return true;
    }
    catch (...) {
        Logger::instance().fail("GET_ORDER_DETAIL 解析失败");
        return false;
    }
}

bool Client::CLTupdateOrderStatus(const std::string& orderId, const std::string& userPhone, int newStatus) {
    json j; j["orderId"] = orderId; j["userPhone"] = userPhone; j["newStatus"] = newStatus;
    // 尝试获取订单详情以附带 shipping_address
    try {
        Order tmp;
        if (CLTgetOrderDetail(orderId, userPhone, tmp)) {
            j["shipping_address"] = tmp.getShippingAddress();
        }
        else {
            j["shipping_address"] = std::string("");
        }
    }
    catch (...) {
        j["shipping_address"] = std::string("");
    }

    std::string resp = CLTsendRequest(std::string("UPDATE_ORDER_STATUS ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "updated";
    }
    catch (...) {
        Logger::instance().fail("UPDATE_ORDER_STATUS 解析失败");
        return false;
    }
}

bool Client::CLTreturnSettledOrder(const std::string& orderId, const std::string& userPhone) {
    json j; j["orderId"] = orderId; j["userPhone"] = userPhone;
    // 附带 shipping_address（尝试获取）
    try {
        Order tmp;
        if (CLTgetOrderDetail(orderId, userPhone, tmp)) j["shipping_address"] = tmp.getShippingAddress();
        else j["shipping_address"] = std::string("");
    }
    catch (...) { j["shipping_address"] = std::string(""); }

    std::string resp = CLTsendRequest(std::string("RETURN_SETTLED_ORDER ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) { Logger::instance().fail("RETURN_SETTLED_ORDER 返回错误: " + r.dump()); return false; }
        return true;
    }
    catch (...) {
        Logger::instance().fail("RETURN_SETTLED_ORDER 解析失败");
        return false;
    }
}

bool Client::CLTrepairSettledOrder(const std::string& orderId, const std::string& userPhone) {
    json j; j["orderId"] = orderId; j["userPhone"] = userPhone;
    // 附带 shipping_address（尝试获取）
    try {
        Order tmp;
        if (CLTgetOrderDetail(orderId, userPhone, tmp)) j["shipping_address"] = tmp.getShippingAddress();
        else j["shipping_address"] = std::string("");
    }
    catch (...) { j["shipping_address"] = std::string(""); }

    std::string resp = CLTsendRequest(std::string("REPAIR_SETTLED_ORDER ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) { Logger::instance().fail("REPAIR_SETTLED_ORDER 返回错误: " + r.dump()); return false;
        }
        return true;
    }
    catch (...) {
        Logger::instance().fail("REPAIR_SETTLED_ORDER 解析失败");
        return false;
    }
}

bool Client::CLTdeleteSettledOrder(const std::string& orderId, const std::string& userPhone) {
    json j; j["orderId"] = orderId; j["userPhone"] = userPhone;
    // 附带 shipping_address（尝试获取）
    try {
        Order tmp;
        if (CLTgetOrderDetail(orderId, userPhone, tmp)) j["shipping_address"] = tmp.getShippingAddress();
        else j["shipping_address"] = std::string("");
    }
    catch (...) { j["shipping_address"] = std::string(""); }

    std::string resp = CLTsendRequest(std::string("DELETE_SETTLED_ORDER ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "deleted";
    }
    catch (...) {
        Logger::instance().fail("DELETE_SETTLED_ORDER 解析失败");
        return false;
    }
}

// ---------------- 促销 ----------------
std::vector<std::shared_ptr<PromotionStrategy>> Client::CLTgetAllPromotions() {
    std::vector<std::shared_ptr<PromotionStrategy>> out;
    std::string resp = CLTsendRequest("GET_ALL_PROMOTIONS");
    if (resp.empty()) return out;
    try {
        auto arr = nlohmann::json::parse(resp);
        if (!arr.is_array()) return out;
        for (const auto& e : arr) {
            try {
                // e["policy"] 是 JSON 对象
                if (e.contains("policy")) {
                    auto strat = PromotionStrategy::fromJson(e["policy"]);
                    out.push_back(strat);
                }
            } catch (const std::exception&) {
                // 跳过解析失败的策略
                continue;
            }
        }
    } catch (...) {}
    return out;
}

std::vector<std::shared_ptr<PromotionStrategy>> Client::CLTgetPromotionsByProductId(int productId) {
    std::vector<std::shared_ptr<PromotionStrategy>> out;
    std::string req = std::string("GET_PROMOTIONS_BY_PRODUCT_ID ") + std::to_string(productId);
    std::string resp = CLTsendRequest(req);
    if (resp.empty()) return out;
    try {
        auto arr = nlohmann::json::parse(resp);
        if (!arr.is_array()) return out;
        for (const auto& e : arr) {
            try {
                if (e.contains("policy")) {
                    auto strat = PromotionStrategy::fromJson(e["policy"]);
                    out.push_back(strat);
                }
            } catch (...) { continue; }
        }
    } catch (...) {}
    return out;
}

bool Client::CLTaddPromotion(const nlohmann::json& promotion) {
    try {
        std::string req = std::string("ADD_PROMOTION ") + promotion.dump();
        std::string resp = CLTsendRequest(req);
        if (resp.empty()) return false;
        auto j = nlohmann::json::parse(resp);
        if (j.is_object() && j.contains("result") && j["result"] == "added") return true;
        return false;
    } catch (...) {
        Logger::instance().fail("CLTaddPromotion 解析失败");
        return false;
    }
}

bool Client::CLTupdatePromotion(const std::string& name, const nlohmann::json& promotion) {
    try {
        nlohmann::json j = promotion;
        j["name"] = name;
        std::string req = std::string("UPDATE_PROMOTION ") + j.dump();
        std::string resp = CLTsendRequest(req);
        if (resp.empty()) return false;
        auto r = nlohmann::json::parse(resp);
        if (r.is_object() && r.contains("result") && r["result"] == "updated") return true;
        return false;
    } catch (...) {
        Logger::instance().fail("CLTupdatePromotion 解析失败");
        return false;
    }
}

bool Client::CLTdeletePromotion(const std::string& name) {
    try {
        nlohmann::json j; j["name"] = name;
        std::string req = std::string("DELETE_PROMOTION ") + j.dump();
        std::string resp = CLTsendRequest(req);
        if (resp.empty()) return false;
        auto r = nlohmann::json::parse(resp);
        if (r.is_object() && r.contains("result") && r["result"] == "deleted") return true;
        return false;
    } catch (...) {
        Logger::instance().fail("CLTdeletePromotion 解析失败");
        return false;
    }
}

std::vector<nlohmann::json> Client::CLTgetAllPromotionsRaw() {
    std::vector<nlohmann::json> out;
    try {
        std::string resp = CLTsendRequest("GET_ALL_PROMOTIONS");
        if (resp.empty()) {
            Logger::instance().info("CLTgetAllPromotionsRaw: empty response");
            return out;
        }
        auto arr = nlohmann::json::parse(resp);
        if (!arr.is_array()) {
            Logger::instance().warn("CLTgetAllPromotionsRaw: response is not an array: " + arr.dump());
            return out;
        }
        for (const auto& e : arr) {
            if (e.is_object()) out.push_back(e);
            else {
                // 保持兼容性：把非对象条目封装为对象
                nlohmann::json wrapper;
                wrapper["value"] = e;
                out.push_back(wrapper);
            }
        }
        Logger::instance().info(std::string("CLTgetAllPromotionsRaw: loaded ") + std::to_string(out.size()) + " promotions");
    } catch (const std::exception& ex) {
        Logger::instance().fail(std::string("CLTgetAllPromotionsRaw parse failed: ") + ex.what());
    } catch (...) {
        Logger::instance().fail("CLTgetAllPromotionsRaw unknown error");
    }
    return out;
}
