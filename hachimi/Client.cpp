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
    std::string resp = CLTsendRequest(std::string("GET_CART ") + userPhone);
    if (resp.empty()) return cart;
    try {
        auto j = json::parse(resp);
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
                    Logger::instance().warn("解析购物车项失败: " + it.dump());
                }
            }
        }
    }
    catch (...) {
        Logger::instance().fail("GET_CART 解析失败");
    }
    return cart;
}

bool Client::CLTsaveCartForUser(const TemporaryCart& cart) {
    json root;
    root["userPhone"] = cart.user_phone;
    json cj;
    cj["cart_id"] = cart.cart_id;
    cj["user_phone"] = cart.user_phone;
    cj["shipping_address"] = cart.shipping_address;
    cj["discount_policy"] = cart.discount_policy;
    cj["total_amount"] = cart.total_amount;
    cj["discount_amount"] = cart.discount_amount;
    cj["final_amount"] = cart.final_amount;
    cj["is_converted"] = cart.is_converted;
    cj["items"] = json::array();
    for (const auto& it : cart.items) {
        json ji;
        ji["good_id"] = it.good_id;
        ji["good_name"] = it.good_name;
        ji["price"] = it.price;
        ji["quantity"] = it.quantity;
        ji["subtotal"] = it.subtotal;
        cj["items"].push_back(ji);
    }
    root["cart"] = cj;
    std::string resp = CLTsendRequest(std::string("SAVE_CART ") + root.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "saved";
    }
    catch (...) {
        Logger::instance().fail("SAVE_CART 解析失败");
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
    json j;
    j["orderId"] = order.getOrderId();
    j["userPhone"] = order.getUserPhone();
    j["status"] = order.getStatus();
    j["discountPolicy"] = order.getDiscountPolicy();
    if (!order.getItems().empty()) {
        const OrderItem& it = order.getItems().front();
        j["productName"] = it.getGoodName();
        j["productId"] = it.getGoodId();
        j["quantity"] = it.getQuantity();
    }
    else {
        j["productName"] = "";
        j["productId"] = 0;
        j["quantity"] = 0;
    }
    std::string resp = CLTsendRequest(std::string("ADD_SETTLED_ORDER ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        return r.value("result", std::string("")) == "added";
    }
    catch (...) {
        Logger::instance().fail("ADD_SETTLED_ORDER 解析失败");
        return false;
    }
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
            std::string oid = e.value("order_id", std::string(""));
            int status = e.value("status", 0);
            Order o(TemporaryCart(), oid, status);
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
        outOrder = Order(); // default then fill if possible via TemporaryCart ctor is used elsewhere
        // Fill fields if matching structure exists
        // set basic fields if present
        // Note: Order 的具体可写接口有限，这 keeps it conservative:
        // 如果需要更完整填充，请在 Order 类添加对应 setter 或构造器
        return true;
    }
    catch (...) {
        Logger::instance().fail("GET_ORDER_DETAIL 解析失败");
        return false;
    }
}

bool Client::CLTupdateOrderStatus(const std::string& orderId, const std::string& userPhone, int newStatus) {
    json j; j["orderId"] = orderId; j["userPhone"] = userPhone; j["newStatus"] = newStatus;
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
    std::string resp = CLTsendRequest(std::string("REPAIR_SETTLED_ORDER ") + j.dump());
    if (resp.empty()) return false;
    try {
        auto r = json::parse(resp);
        if (r.is_object() && r.contains("error")) { Logger::instance().fail("REPAIR_SETTLED_ORDER 返回错误: " + r.dump()); return false; }
        return true;
    }
    catch (...) {
        Logger::instance().fail("REPAIR_SETTLED_ORDER 解析失败");
        return false;
    }
}

bool Client::CLTdeleteSettledOrder(const std::string& orderId, const std::string& userPhone) {
    json j; j["orderId"] = orderId; j["userPhone"] = userPhone;
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

// ---------------- 促销（占位） ----------------
std::vector<PromotionStrategy> Client::CLTgetAllPromotions() { return {}; }
std::vector<PromotionStrategy> Client::CLTgetPromotionsByProductId(int) { return {}; }


