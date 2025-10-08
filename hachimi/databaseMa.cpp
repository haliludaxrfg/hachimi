#include "databaseManager.h"
using nlohmann::json;

DatabaseManager::DatabaseManager(const std::string& host, const std::string& user,
    const std::string& password, const std::string& database, unsigned int port)
    : host_(host), user_(user), password_(password), database_(database), port_(port), connection_(nullptr) {}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

bool DatabaseManager::connect() {
    // 初始化 MYSQL 结构体
    connection_ = mysql_init(nullptr);
    if (!connection_) {
        std::cerr << "mysql_init failed!" << std::endl;
        return false;
    }
    // 连接数据库
    if (!mysql_real_connect(connection_, host_.c_str(), user_.c_str(), password_.c_str(),
        database_.c_str(), port_, nullptr, 0)) {
        std::cerr << "mysql_real_connect failed: " << mysql_error(connection_) << std::endl;
        mysql_close(connection_);
        connection_ = nullptr;
        return false;
    }
    return true;
}

void DatabaseManager::disconnect() {
    if (connection_) {
        mysql_close(connection_);
        connection_ = nullptr;
    }
}

bool DatabaseManager::initialize() {
    return connect();
}

bool DatabaseManager::isConnected() const {
    return connection_ != nullptr;
}

// executeQuery函数定义
bool DatabaseManager::executeQuery(const std::string& query) {
    if (!connection_) return false;
    if (mysql_query(connection_, query.c_str()) == 0) {
        return true;
    } else {
        std::cerr << "MySQL query error: " << mysql_error(connection_) << std::endl;
        return false;
    }
}

// 将 executeSelect 实现为静态成员函数
MYSQL_RES* DatabaseManager::executeSelect(const std::string& query) {
    if (!connection_) return nullptr;
    if (mysql_query(connection_, query.c_str()) != 0) {
        std::cerr << "MySQL select query error: " << mysql_error(connection_) << std::endl;
        return nullptr;
    }
    return mysql_store_result(connection_);
}

bool DatabaseManager::savePromotionStrategy(const std::string& name, const std::string& type,
    const std::string& config, const std::string& conditions) {
    // 1. 组装json
    json js;
    js["type"] = type;
    js["config"] = config;
    if (!conditions.empty()) {
        js["conditions"] = conditions;
    }
    std::string sql = "INSERT INTO promotion_strategies (name, policy_detail) VALUES ('" +
        name + "', '" + js.dump() + "')";
    return this->executeQuery(sql);
}

bool DatabaseManager::updatePromotionStrategy(const std::string& name, bool is_active) {
    // 更新促销策略的激活状态
    std::string sql = "UPDATE promotion_strategies SET is_active = " + std::to_string(is_active ? 1 : 0) +
                      " WHERE name = '" + name + "'";
    return this->executeQuery(sql);
}

std::map<std::string, std::string> DatabaseManager::loadPromotionStrategy(const std::string& name) {
    std::map<std::string, std::string> result;
    std::string sql = "SELECT id, name, policy_detail FROM promotion_strategies WHERE name='" + name + "'";
    MYSQL_RES* res = executeSelect(sql);
    if (!res) return result;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        result["id"] = row[0] ? row[0] : "";
        result["name"] = row[1] ? row[1] : "";
        result["policy_detail"] = row[2] ? row[2] : "";
    }
    mysql_free_result(res);
    return result;
}
std::vector<std::map<std::string, std::string>> DatabaseManager::loadAllPromotionStrategies(bool active_only) {
    std::vector<std::map<std::string, std::string>> result;
    std::string sql = "SELECT id, name, policy_detail FROM promotion_strategies";
    if (active_only) {
        sql += " WHERE is_active = 1";
    }
    MYSQL_RES* res = executeSelect(sql);
    if (!res) return result;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        std::map<std::string, std::string> item;
        item["id"] = row[0] ? row[0] : "";
        item["name"] = row[1] ? row[1] : "";
        item["policy_detail"] = row[2] ? row[2] : "";
        result.push_back(item);
    }
    mysql_free_result(res);
    return result;
}